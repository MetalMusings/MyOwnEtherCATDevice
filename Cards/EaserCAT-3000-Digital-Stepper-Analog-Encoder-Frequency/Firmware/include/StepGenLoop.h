// The position/velocity loop of one step generator channel, once per servo cycle:
// LinuxCNC stepgen's update_freq() without double precision floating point.
//
// The STM32F407's FPU is single precision only; double is done in software (40 to
// more than 100 cycles per operation). float alone is not good enough for absolute
// positions: at a million steps its resolution is 0.06 steps, which the loop would
// turn into velocity noise. So absolute positions stay 64 bit fixed point (steps with
// PICKOFF fraction bits), and only differences of them,
// velocities and accelerations are float.
#ifndef STEPGENLOOP_H
#define STEPGENLOOP_H

#include "StepGenChannel.h"
#include "StepPlanner.h"

struct StepLoopTiming {
  float dt;          // servo period, s
  float recipDt;     // 1 / dt
};

static const int64_t STEPGEN_FX_ONE = 1LL << PICKOFF;

// Fixed point steps <-> integer steps (rounded) and float steps
static inline int64_t stepgenFxFromSteps(int64_t steps) { return steps * STEPGEN_FX_ONE; }
static inline int64_t stepgenFxRound(int64_t fx) { return (fx + (STEPGEN_FX_ONE >> 1)) >> PICKOFF; }
static inline float stepgenFxToFloat(int64_t fx) { return (float)fx * (1.0f / (float)STEPGEN_FX_ONE); }

// One servo cycle of one channel: sets freq and accel from pos_cmd (position mode) or
// vel_cmd (velocity mode), maxvel and maxaccel.
void stepgenUpdateFreq(stepgen_t *sg, const StepLoopTiming &k);

// One cycle of one axis with the step timers: the loop continues from where the queue
// ends (STEP_LEAD_TICKS after now, so a position command is extrapolated by that much),
// then the next window is planned. nowT: the axis' timer time now. Underruns and the
// flush on disable are done by the caller, with interrupts off.
struct StepAxisState {
    int64_t prevCmd = 0;   // the last raw position command
    bool prevValid = false;
};
void stepAxisCycle(stepgen_t *sg, StepAxisState &ax, StepPlanState &ps, StepQueue &q,
                   const StepTimingTicks &tm, const StepLoopTiming &k, int64_t nowT,
                   int64_t cycleTicks, float freqCap);

// Position (fixed point steps) from the accumulator
static inline int64_t stepgenPosition(const stepgen_t *sg)
{
    return sg->accum - (1LL << (PICKOFF - 1));
}

#endif
