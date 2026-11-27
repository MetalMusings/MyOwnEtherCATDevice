// Step/dir outputs of the four axes, made by hardware timers (see StepPlanner.h).
//
//   axis  step         timer      dir
//   1     PA11 (AF1)   TIM1 CH4   PA12
//   2     PC9  (AF3)   TIM8 CH4   PC8
//   3     PD12 (AF2)   TIM4 CH1   PD11
//   4     PE5  (AF3)   TIM9 CH1   PE4
//
// All four count at 80 MHz (12.5 ns). Each update event (start of a timer period) has a
// short interrupt that loads the next period from the axis' queue, sets the direction
// pin and counts the step. The queues are in the CCM RAM.
#ifndef STEPTIMERS_H
#define STEPTIMERS_H

#include <stdint.h>

#include "StepGenChannel.h"
#include "StepGenLoop.h"
#include "StepPlanner.h"

#define STEP_AXES 4
#define STEP_TIMING_DEFAULT_NS 5000u  // step length, step space, direction setup and hold: 0 = this

struct StepDiagnostics {
    uint32_t isrCycles;     // interrupt CPU cycles since the last take
    uint32_t isrMaxCycles;  // longest interrupt
    uint32_t planCycles;    // planning CPU cycles in the last update()
    uint32_t late[4];       // per axis: the queue ran empty while it was moving
    int32_t leadMinTicks[4];  // per axis: shortest queue lead at a planning, since the reset
};

class StepTimers {
  public:
    stepgen_t sg[STEP_AXES];  // position loop state (LinuxCNC stepgen's update_freq)

    // Timers, pins, queues and interrupts. vectors: the RAM vector table to put the
    // handlers in.
    void begin(uint32_t *vectors);

    void setTiming(int ch, uint32_t stepLenNs, uint32_t stepSpaceNs, uint32_t dirSetupNs,
                   uint32_t dirHoldNs);
    void setChannel(int ch, bool enable, bool posMode, int64_t posCmd, float velCmd,
                    float maxVel, float maxAccel, bool resync);
    // Once per cycle: position loop and planning of all axes
    void update(uint32_t cycleNs);

    // Steps output (signed), safe to read from an interrupt
    int32_t count(int ch) const { return q[ch].emitted; }
    // Position feedback, fixed point steps (StepGenLoop.h)
    int64_t position(int ch) const { return (int64_t)q[ch].emitted << 28; }
    float velocity(int ch) const { return sg[ch].freq; }
    // Highest step rate of an axis: its step timing and the queue (at the cycle time)
    uint32_t maxFrequency(int ch) const { return maxFreq[ch]; }

    // Diagnostics: interrupt cycles are summed until taken
    StepDiagnostics takeDiagnostics();
    void resetDiagnostics();      // maxima and minima
    void resetLate(int ch) { late[ch] = 0; }

    // Called by the interrupt handlers
    void isr(int ch);

  private:
    StepQueue q[STEP_AXES];
    StepPlanState plan[STEP_AXES];
    StepTimingTicks tm[STEP_AXES];
    uint32_t timingNs[STEP_AXES][4] = {};
    uint32_t maxFreq[STEP_AXES] = {};
    StepAxisState axis[STEP_AXES];
    bool wasEnabled[STEP_AXES] = {};
    uint64_t startDwt[STEP_AXES] = {};
    uint64_t dwtHigh = 0;
    uint32_t dwtLast = 0;
    volatile uint32_t isrCycles = 0, isrMaxCycles = 0;
    uint32_t planCycles = 0;
    uint32_t late[STEP_AXES] = {};
    int32_t leadMin[STEP_AXES] = {0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF};

    uint64_t dwt64();
};

extern StepTimers Steps;

#endif
