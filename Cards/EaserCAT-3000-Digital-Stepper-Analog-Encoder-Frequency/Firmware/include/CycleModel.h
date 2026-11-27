// What cycle time a base period allows, and the step rate it gives. Free of Arduino
// headers, so it can be tested on the host.
//
// The cycle's work runs in the main loop; the base period interrupt (step generator)
// preempts it. With all four axes stepping, the interrupt takes CYCLE_ISR_NS every base
// period, and the cycle's own work (SPI, PDO, the axes) CYCLE_WORK_NS. The cycle then
// takes CYCLE_WORK_NS / (1 - CYCLE_ISR_NS / base period). Measured in OP at 160 MHz,
// 2026-11-27: 4.8 us per interrupt, 120 us of work; at a 10 us base period the model
// gives 231 us, the cycle measured 231 us. Measured with four axes at 90 % of their step
// rate (cycle, share of a 1 ms / 0.5 ms cycle):
//   base 10 us 231 us, 9 us 260, 8 us 290, 7 us 380 (38 % / 76 %), 6 us 657 (66 % / fails),
//   5.5 us fails at both.
#ifndef CYCLEMODEL_H
#define CYCLEMODEL_H

#include <stdint.h>

#define CYCLE_ISR_NS 4900            // base period interrupt, four axes stepping
#define CYCLE_WORK_NS 120000         // one cycle without the interrupt
#define CYCLE_MARGIN_PERCENT 20      // for the mailbox, jitter and the other interrupts
#define CYCLE_MAX_ISR_LOAD_PERMILLE 820  // 6 us (817) worked, 5.5 us (890) did not
#define CYCLE_IMPOSSIBLE 0xFFFFFFFFu

// Share of the CPU the interrupt takes at this base period, in 0.1 %
static inline uint32_t cycleModelIsrLoad(uint32_t basePeriodNs)
{
    if (basePeriodNs == 0) return 1000;
    uint32_t l = (uint32_t)((uint64_t)CYCLE_ISR_NS * 1000u / basePeriodNs);
    return l > 1000 ? 1000 : l;
}

// Shortest cycle time (ns) that works at this base period with all axes stepping,
// rounded up to 10 us; CYCLE_IMPOSSIBLE when the base period itself is too short.
static inline uint32_t cycleModelMinCycle(uint32_t basePeriodNs)
{
    uint32_t load = cycleModelIsrLoad(basePeriodNs);
    if (load > CYCLE_MAX_ISR_LOAD_PERMILLE) return CYCLE_IMPOSSIBLE;
    uint64_t need = (uint64_t)CYCLE_WORK_NS * 1000u / (1000u - load);
    need = need * (100u + CYCLE_MARGIN_PERCENT) / 100u;
    need = (need + 9999u) / 10000u * 10000u;
    if (need < 2ull * basePeriodNs) need = 2ull * basePeriodNs;
    return need > 0xFFFFFFF0ull ? CYCLE_IMPOSSIBLE : (uint32_t)need;
}

// Shortest base period (ns) the interrupt allows at all
static inline uint32_t cycleModelMinBasePeriod(void)
{
    return (uint32_t)(((uint64_t)CYCLE_ISR_NS * 1000u + CYCLE_MAX_ISR_LOAD_PERMILLE - 1) /
                      CYCLE_MAX_ISR_LOAD_PERMILLE);
}

// Highest step rate (Hz) of step/dir with these timings (ns), each rounded up to whole
// base periods, step length and space at least one base period each
static inline uint32_t cycleModelMaxStepFrequency(uint32_t basePeriodNs, uint32_t stepLen,
                                                  uint32_t stepSpace)
{
    if (basePeriodNs == 0) return 0;
    uint32_t len = stepLen ? (stepLen + basePeriodNs - 1) / basePeriodNs : 1;
    uint32_t space = stepSpace ? (stepSpace + basePeriodNs - 1) / basePeriodNs : 1;
    return (uint32_t)(1000000000ull / ((uint64_t)(len + space) * basePeriodNs));
}

#endif
