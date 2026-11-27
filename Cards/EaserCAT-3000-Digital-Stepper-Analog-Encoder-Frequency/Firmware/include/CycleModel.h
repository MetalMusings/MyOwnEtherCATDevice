// What cycle time the step rates allow. Free of Arduino headers, so it can be tested on
// the host.
//
// The cycle's work runs in the main loop (SPI, PDO, the axes: CYCLE_WORK_NS). The step
// timers (StepTimers.h) cost CPU per step: an update interrupt for every step, and the
// planning of it in the cycle (CYCLE_STEP_NS together). With R steps per second over all
// axes the cycle then takes CYCLE_WORK_NS / (1 - R * CYCLE_STEP_NS), plus margin.
// Measured in OP, 2026-11-27, four axes in csv, 2 us step length and space (cycle used):
//   1 ms: 4 x 100 kHz 39 %, 150 kHz 58 %, 175 kHz 71 %, 200 kHz 89 % (one late step)
//   0.5 ms: 4 x 100 kHz 65 %, 125 kHz 76 %, 150 kHz 94 % (99 % at most)
//   0.25 ms: 4 x 45 kHz 84 %
// The model accepts each of these but 0.5 ms at 4 x 150 kHz and 1 ms at 4 x 200 kHz.
#ifndef CYCLEMODEL_H
#define CYCLEMODEL_H

#include <stdint.h>

#define CYCLE_WORK_NS 160000          // one cycle without the steps (incl. planning's fixed part)
#define CYCLE_STEP_NS 1100            // CPU per step: interrupt (0.45 us) and planning
#define CYCLE_MARGIN_PERCENT 20       // for the mailbox, jitter and the other interrupts
#define CYCLE_MAX_STEP_LOAD_PERMILLE 900
#define CYCLE_MIN_NS 100000           // shortest cycle accepted at all
#define CYCLE_IMPOSSIBLE 0xFFFFFFFFu

// Share of the CPU the step generation takes at R steps/s over all axes, in 0.1 %
static inline uint32_t cycleModelStepLoad(uint32_t totalStepRateHz)
{
    uint64_t l = (uint64_t)totalStepRateHz * CYCLE_STEP_NS / 1000000u;
    return l > 1000 ? 1000u : (uint32_t)l;
}

// Shortest cycle time (ns) with the axes at these step rates, rounded up to 10 us;
// CYCLE_IMPOSSIBLE when the steps alone would take too much of the CPU
static inline uint32_t cycleModelMinCycle(uint32_t totalStepRateHz)
{
    uint32_t load = cycleModelStepLoad(totalStepRateHz);
    if (load > CYCLE_MAX_STEP_LOAD_PERMILLE) return CYCLE_IMPOSSIBLE;
    uint64_t need = (uint64_t)CYCLE_WORK_NS * 1000u / (1000u - load);
    need = need * (100u + CYCLE_MARGIN_PERCENT) / 100u;
    need = (need + 9999u) / 10000u * 10000u;
    if (need < CYCLE_MIN_NS) need = CYCLE_MIN_NS;
    return (uint32_t)need;
}

#endif
