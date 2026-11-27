// The position/velocity loop of one step generator channel, once per servo cycle:
// LinuxCNC stepgen's update_freq() without double precision floating point.
//
// The STM32F407's FPU is single precision only; double is done in software (40 to
// more than 100 cycles per operation). float alone is not good enough for absolute
// positions: at a million steps its resolution is 0.06 steps, which the loop would
// turn into velocity noise. So absolute positions stay 64 bit fixed point (steps with
// PICKOFF fraction bits, as the DDS accumulator), and only differences of them,
// velocities and accelerations are float.
#ifndef STEPGENLOOP_H
#define STEPGENLOOP_H

#include "StepGenChannel.h"

struct StepLoopTiming {
  float dt;          // servo period, s
  float recipDt;     // 1 / dt
  float freqscale;   // steps/s -> addval (2^PICKOFF x base period)
  float accelscale;  // steps/s^2 -> deltalim (freqscale x base period)
};

static const int64_t STEPGEN_FX_ONE = 1LL << PICKOFF;

// Fixed point steps <-> integer steps (rounded) and float steps
static inline int64_t stepgenFxFromSteps(int64_t steps) { return steps * STEPGEN_FX_ONE; }
static inline int64_t stepgenFxRound(int64_t fx) { return (fx + (STEPGEN_FX_ONE >> 1)) >> PICKOFF; }
static inline float stepgenFxToFloat(int64_t fx) { return (float)fx * (1.0f / (float)STEPGEN_FX_ONE); }

// One servo cycle of one channel: sets freq, target_addval and deltalim from
// pos_cmd (position mode) or vel_cmd (velocity mode), maxvel and maxaccel.
// The timing parameters (step_len, step_space) must already be whole base periods.
void stepgenUpdateFreq(stepgen_t *sg, const StepLoopTiming &k);

// Position feedback (fixed point steps) from the accumulator
static inline int64_t stepgenPosition(const stepgen_t *sg)
{
    long long a, b;
    do
    {
        a = sg->accum;  // 64 bit: make_pulses() may change it half-way through a read
        b = sg->accum;
    } while (a != b);
    return a - (1LL << (PICKOFF - 1));
}

#endif
