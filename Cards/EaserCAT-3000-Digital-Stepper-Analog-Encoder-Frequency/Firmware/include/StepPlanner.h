// Step generation with hardware timers: the planner (once per servo cycle) and the queue
// the timer interrupt reads. Free of Arduino headers, so it can be tested on the host.
//
// Each axis has a timer in PWM mode 1 with ARR and CCR preloaded: every timer period
// starts with a step pulse of CCR ticks (CCR = 0: no pulse). The queue holds one entry
// per period. At every update event the interrupt takes the next entry into the preload
// registers (it becomes active at the following update), applies the direction action
// of the period that has just started and counts its step.
//
// The planner works one window ahead: each cycle it extends the queue to about one
// cycle plus STEP_LEAD_TICKS after "now". The velocity ramps from where the queue ends to
// the target velocity of the position loop, at its acceleration limit, as the step
// generator's DDS did, but every step is placed to the timer tick (12.5 ns) instead of
// to a base period. Positions are the DDS accumulator's: steps << 28, half a step offset;
// a step is made where the accumulator crosses a multiple of 2^28.
#ifndef STEPPLANNER_H
#define STEPPLANNER_H

#include <stdint.h>

#define STEP_TICK_HZ 80000000u          // timer ticks per second (12.5 ns)
#define STEP_MIN_PERIOD_TICKS 160u      // 2 us: the interrupt must take the next entry in time
#define STEP_MAX_PERIOD_TICKS 65536u    // 16 bit timers
#define STEP_MAX_PULSE_TICKS 16383u     // 14 bits in an entry, 204 us
#define STEP_DIR_MARGIN_TICKS 80u       // 1 us: the direction pin is set by the interrupt
#define STEP_LEAD_TICKS 24000u          // 300 us: the queue runs this far ahead of the cycle
#define STEP_TAIL_TICKS 4000u           // 50 us: longest last piece of a period (see below)
#define STEP_UNDERRUN_TICKS 8000u       // idle period the interrupt makes when the queue is empty
#define STEP_QUEUE_LEN 2048u            // entries per axis, power of two
#define STEP_QUEUE_MASK (STEP_QUEUE_LEN - 1u)

// Queue entry: bits 0..15 period ticks - 1 (ARR), 16..29 pulse ticks (CCR, 0 = no pulse),
// 30..31 direction action at the start of the period
#define STEP_E_PULSE_SHIFT 16
#define STEP_E_PULSE_MASK 0x3FFFu
#define STEP_E_DIR_POS (1u << 30)
#define STEP_E_DIR_NEG (2u << 30)
static inline uint32_t stepEntryTicks(uint32_t e) { return (e & 0xFFFFu) + 1u; }
static inline uint32_t stepEntryPulse(uint32_t e) { return (e >> STEP_E_PULSE_SHIFT) & STEP_E_PULSE_MASK; }
static inline uint32_t stepEntryDir(uint32_t e) { return e >> 30; }

// What the interrupt keeps per axis. head is written by the planner only, the rest by
// the interrupt only (and by stepQueueFlush with interrupts off).
struct StepQueue {
  uint32_t *buf;
  volatile uint32_t head;      // next entry the planner writes
  volatile uint32_t tail;      // next entry the interrupt takes
  uint32_t preload;            // entry in the preload registers
  int32_t dirActive;           // direction of the period running now, +1 / -1
  int32_t dirLoaded;           // direction after the preload entry
  volatile int32_t emitted;    // steps output, signed: the position feedback
  int32_t loadedCount;         // steps after the preload entry
  int64_t loadedEnd;           // ticks: end of the preload period
  volatile uint32_t underrunTicks;  // idle ticks made because the queue was empty
  volatile uint32_t underruns;
};

// Set up a queue as the timer starts: the active period and the preload are both
// 'idle' ticks long, the first entry starts after them
void stepQueueInit(StepQueue &q, uint32_t *buf, uint32_t idleTicks);

// One update event, in the interrupt: returns the entry for the preload registers;
// *activeDir is the direction action of the period that has just started (0 = none)
static inline uint32_t stepQueueAdvance(StepQueue &q, uint32_t *activeDir)
{
    const uint32_t act = q.preload;
    const uint32_t da = stepEntryDir(act);
    if (da) q.dirActive = (da == 1) ? 1 : -1;
    if (stepEntryPulse(act)) q.emitted += q.dirActive;
    *activeDir = da;
    uint32_t e;
    const uint32_t t = q.tail;
    if (t != q.head) {
        e = q.buf[t & STEP_QUEUE_MASK];
        q.tail = t + 1;
    } else {
        e = STEP_UNDERRUN_TICKS - 1u;
        q.underrunTicks += STEP_UNDERRUN_TICKS;
        q.underruns++;
    }
    const uint32_t de = stepEntryDir(e);
    if (de) q.dirLoaded = (de == 1) ? 1 : -1;
    if (stepEntryPulse(e)) q.loadedCount += q.dirLoaded;
    q.loadedEnd += stepEntryTicks(e);
    q.preload = e;
    return e;
}

// Step timing in ticks
struct StepTimingTicks {
    uint32_t len, space, setup, hold;
};

// The interrupt loads one period ahead, so the queue lasts until the start of its last
// period, not its end. A long period is therefore split so that its last piece is at
// most STEP_TAIL_TICKS: the queue then always lasts until about STEP_LEAD_TICKS -
// STEP_CARRY_TICKS - STEP_TAIL_TICKS (150 us) after the next cycle's planning time.

// Planner state of one axis
struct StepPlanState {
    int64_t end;       // ticks: where the next window starts (the profile's time line)
    int64_t qEnd;      // ticks: end of the periods in the queue
    bool open;         // the last event's period is not in the queue yet: it ends at the
    int64_t openStart; //   next event, so that a window boundary does not move a step
    uint32_t openFlags;
    bool openPulse;
    int64_t accum;     // accumulator (position) at 'end'
    float vel;         // steps/s at 'end'
    int32_t dir;       // direction pin after the planned events, +1 / -1
    int64_t lastStep;  // ticks: start of the last step pulse
    int64_t lastEvent; // ticks: start of the last period with a step or direction change
    uint32_t steps;    // steps in the last window (for the diagnostics)
};

#define STEP_CARRY_TICKS 8000u  // 100 us: the open period starts at most this long before the window end

void stepPlanInit(StepPlanState &s, const StepQueue &q, int64_t accum);

// Shortest period that may follow an event (pulse or direction change)
static inline uint32_t stepMinTail(const StepTimingTicks &tm, bool pulse)
{
    uint32_t m = STEP_MIN_PERIOD_TICKS;
    if (pulse && tm.len + 1u > m) m = tm.len + 1u;
    return m;
}

// Plan from s.end to 'until': ramp the velocity from s.vel to vTarget at accel
// (steps/s^2, > 0), place the steps and write the periods into the queue (published at
// the end). Returns the entries written. The window is shortened if the queue has not
// enough room.
uint32_t stepPlanWindow(StepPlanState &s, StepQueue &q, const StepTimingTicks &tm, float vTarget,
                        float accel, int64_t until);

// Take the idle the interrupt made into the plan (with interrupts off around the read):
// everything after the queue's end moves later by it
static inline void stepPlanTakeUnderrun(StepPlanState &s, uint32_t underrunTicks)
{
    s.end += underrunTicks;
    s.qEnd += underrunTicks;
    s.openStart += underrunTicks;
    s.lastStep += underrunTicks;
    s.lastEvent += underrunTicks;
}

// Drop what is queued and not yet loaded: the axis stops after the preload period.
// Call with interrupts off. The plan restarts where the interrupt is.
void stepQueueFlush(StepQueue &q, StepPlanState &s);

#endif
