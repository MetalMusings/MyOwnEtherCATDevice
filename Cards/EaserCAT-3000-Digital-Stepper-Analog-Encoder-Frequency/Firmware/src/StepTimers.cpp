// See StepTimers.h
#pragma GCC optimize("O2")
#include "StepTimers.h"

#include <Arduino.h>
#include <string.h>

#include "StepGenLoop.h"

StepTimers Steps;

namespace {

struct AxisHw {
    TIM_TypeDef *tim;
    volatile uint32_t *ccr;
    IRQn_Type irq;
    GPIO_TypeDef *stepPort;
    uint8_t stepPin, af;
    GPIO_TypeDef *dirPort;
    uint8_t dirPin;
    bool apb2, advanced;
    uint8_t channel;  // 1 or 4
};

const AxisHw HW[STEP_AXES] = {
    {TIM1, &TIM1->CCR4, TIM1_UP_TIM10_IRQn, GPIOA, 11, 1, GPIOA, 12, true, true, 4},
    {TIM8, &TIM8->CCR4, TIM8_UP_TIM13_IRQn, GPIOC, 9, 3, GPIOC, 8, true, true, 4},
    {TIM4, &TIM4->CCR1, TIM4_IRQn, GPIOD, 12, 2, GPIOD, 11, false, false, 1},
    {TIM9, &TIM9->CCR1, TIM1_BRK_TIM9_IRQn, GPIOE, 5, 3, GPIOE, 4, true, false, 1},
};
uint32_t dirMask[STEP_AXES];

// The queues in the CCM RAM: 64 KB at 0x10000000 that nothing else uses (the linker
// script puts nothing there). Only the CPU reaches it, which is all the queues need.
uint32_t *const QUEUE_MEM = (uint32_t *)0x10000000;
const uint32_t IDLE_TICKS = 8000;  // 100 us periods until the first plan

void pinAf(GPIO_TypeDef *g, uint8_t pin, uint8_t af)
{
    g->AFR[pin >> 3] = (g->AFR[pin >> 3] & ~(0xFu << ((pin & 7u) * 4u))) | ((uint32_t)af << ((pin & 7u) * 4u));
    g->OTYPER &= ~(1u << pin);
    g->OSPEEDR = (g->OSPEEDR & ~(3u << (2u * pin))) | (2u << (2u * pin));
    g->MODER = (g->MODER & ~(3u << (2u * pin))) | (2u << (2u * pin));
}

void pinOut(GPIO_TypeDef *g, uint8_t pin, bool high)
{
    g->BSRR = high ? (1u << pin) : (1u << (pin + 16));
    g->OTYPER &= ~(1u << pin);
    g->OSPEEDR = (g->OSPEEDR & ~(3u << (2u * pin))) | (2u << (2u * pin));
    g->MODER = (g->MODER & ~(3u << (2u * pin))) | (1u << (2u * pin));
}

uint32_t timerClock(bool apb2)
{
    if (apb2) {
        uint32_t p = HAL_RCC_GetPCLK2Freq();
        return ((RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV1) ? p : 2u * p;
    }
    uint32_t p = HAL_RCC_GetPCLK1Freq();
    return ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) ? p : 2u * p;
}

inline uint32_t nsToTicks(uint32_t ns) { return (ns * (STEP_TICK_HZ / 1000000u) + 999u) / 1000u; }

}  // namespace

// The update interrupt of one axis: next period into the preload registers
inline __attribute__((always_inline)) void StepTimers::isr(int ch)
{
    const AxisHw &h = HW[ch];
    const uint32_t t0 = DWT->CYCCNT;
    h.tim->SR = ~TIM_SR_UIF;
    uint32_t da;
    const uint32_t e = stepQueueAdvance(q[ch], &da);
    if (da) h.dirPort->BSRR = (da == 1) ? dirMask[ch] : dirMask[ch] << 16;
    h.tim->ARR = e & 0xFFFFu;
    *h.ccr = stepEntryPulse(e);
    const uint32_t d = DWT->CYCCNT - t0;
    isrCycles += d;
    if (d > isrMaxCycles) isrMaxCycles = d;
}

extern "C" {
static void stepIrq1(void) { Steps.isr(0); }
static void stepIrq2(void) { Steps.isr(1); }
static void stepIrq3(void) { Steps.isr(2); }
static void stepIrq4(void) { Steps.isr(3); }
}

uint64_t StepTimers::dwt64()
{
    const uint32_t c = DWT->CYCCNT;
    if (c < dwtLast) dwtHigh += 1ull << 32;
    dwtLast = c;
    return dwtHigh | c;
}

void StepTimers::begin(uint32_t *vectors)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN | RCC_APB2ENR_TIM8EN | RCC_APB2ENR_TIM9EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    (void)RCC->APB1ENR;

    void (*const handlers[STEP_AXES])(void) = {stepIrq1, stepIrq2, stepIrq3, stepIrq4};
    for (int i = 0; i < STEP_AXES; i++) {
        const AxisHw &h = HW[i];
        dirMask[i] = 1u << h.dirPin;
        pinOut(h.dirPort, h.dirPin, true);    // positive
        pinOut(h.stepPort, h.stepPin, false);

        TIM_TypeDef *t = h.tim;
        t->CR1 = 0;
        t->DIER = 0;
        t->PSC = timerClock(h.apb2) / STEP_TICK_HZ - 1u;
        t->ARR = IDLE_TICKS - 1u;
        t->CNT = 0;
        if (h.channel == 4) {
            t->CCMR2 = (t->CCMR2 & ~(TIM_CCMR2_OC4M | TIM_CCMR2_CC4S)) | (6u << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;
            t->CCER = (t->CCER & ~TIM_CCER_CC4P) | TIM_CCER_CC4E;
        } else {
            t->CCMR1 = (t->CCMR1 & ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S)) | (6u << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
            t->CCER = (t->CCER & ~TIM_CCER_CC1P) | TIM_CCER_CC1E;
        }
        *h.ccr = 0;  // PWM mode 1: high while the counter is below CCR, so no pulse
        if (h.advanced) t->BDTR = TIM_BDTR_MOE;
        t->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
        t->EGR = TIM_EGR_UG;  // load PSC, ARR and CCR; URS: no interrupt for it
        t->SR = 0;
        t->DIER = TIM_DIER_UIE;
        pinAf(h.stepPort, h.stepPin, h.af);

        stepQueueInit(q[i], QUEUE_MEM + (uint32_t)i * STEP_QUEUE_LEN, IDLE_TICKS);
        stepPlanInit(plan[i], q[i], 1LL << 27);
        memset((void *)&sg[i], 0, sizeof(sg[i]));
        sg[i].accum = 1LL << 27;
        setTiming(i, 0, 0, 0, 0);

        vectors[16 + h.irq] = (uint32_t)handlers[i];
        NVIC_SetPriority(h.irq, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
        NVIC_EnableIRQ(h.irq);
    }
    // Start: the time line of axis i is (DWT - startDwt[i]) / 2 timer ticks
    __disable_irq();
    for (int i = 0; i < STEP_AXES; i++) {
        startDwt[i] = dwt64();
        HW[i].tim->CR1 |= TIM_CR1_CEN;
    }
    __enable_irq();
}

void StepTimers::setTiming(int ch, uint32_t stepLenNs, uint32_t stepSpaceNs, uint32_t dirSetupNs,
                           uint32_t dirHoldNs)
{
    uint32_t *t = timingNs[ch];
    uint32_t v[4] = {stepLenNs ? stepLenNs : STEP_TIMING_DEFAULT_NS,
                     stepSpaceNs ? stepSpaceNs : STEP_TIMING_DEFAULT_NS,
                     dirSetupNs ? dirSetupNs : STEP_TIMING_DEFAULT_NS,
                     dirHoldNs ? dirHoldNs : STEP_TIMING_DEFAULT_NS};
    if (memcmp(t, v, sizeof(v)) == 0) return;
    memcpy(t, v, sizeof(v));
    StepTimingTicks &k = tm[ch];
    k.len = nsToTicks(v[0]);
    if (k.len < 80u) k.len = 80u;  // 1 us
    if (k.len > STEP_MAX_PULSE_TICKS) k.len = STEP_MAX_PULSE_TICKS;
    k.space = nsToTicks(v[1]);
    if (k.space < 80u) k.space = 80u;
    k.setup = nsToTicks(v[2]);
    k.hold = nsToTicks(v[3]);
    // The position loop limits the rate to 1 / (length + space)
    sg[ch].step_len = (int)(k.len * 1000u / (STEP_TICK_HZ / 1000000u));
    sg[ch].step_space = k.space * 1000u / (STEP_TICK_HZ / 1000000u);
}

void StepTimers::setChannel(int ch, bool enable, bool posMode, int64_t posCmd, float velCmd,
                            float maxVel, float maxAccel, bool resync)
{
    stepgen_t *s = &sg[ch];
    if (posMode && (!s->pos_mode || resync)) {
        // No velocity feed forward from an old command in the first cycle
        s->old_pos_cmd = posCmd;
        axis[ch].prevValid = false;
    }
    s->pos_mode = posMode ? 1 : 0;
    s->pos_cmd = posCmd;
    s->vel_cmd = velCmd;
    s->maxvel = maxVel;
    s->maxaccel = maxAccel;
    s->enable = enable ? 1 : 0;
}

void StepTimers::update(uint32_t cycleNs)
{
    const uint32_t t0 = DWT->CYCCNT;
    const uint32_t ratio = SystemCoreClock / STEP_TICK_HZ;
    const int64_t cycleTicks = (int64_t)cycleNs * (STEP_TICK_HZ / 1000000u) / 1000;
    const float dt = (float)cycleNs * 1e-9f;
    const StepLoopTiming k = {dt, 1.0f / dt, 0.0f, 0.0f};
    // The queue holds about a cycle and the lead: limit the rate so that it fits
    float cap = (float)(STEP_QUEUE_LEN - 248u) * (float)STEP_TICK_HZ /
                (float)(cycleTicks + STEP_LEAD_TICKS + STEP_CARRY_TICKS);
    if (cap > (float)STEP_RATE_MAX_HZ) cap = (float)STEP_RATE_MAX_HZ;

    for (int i = 0; i < STEP_AXES; i++) {
        StepQueue &qq = q[i];
        StepPlanState &ps = plan[i];
        stepgen_t &g = sg[i];

        __disable_irq();
        const uint32_t u = qq.underrunTicks;
        qq.underrunTicks = 0;
        __enable_irq();
        if (u) {
            if (ps.vel != 0.0f || ps.steps) late++;
            stepPlanTakeUnderrun(ps, u);
        }
        if (!g.enable && wasEnabled[i]) {  // disabled: stop now, drop what is queued
            __disable_irq();
            stepQueueFlush(qq, ps);
            __enable_irq();
        }
        wasEnabled[i] = g.enable != 0;

        const float timing = 1.0e9f / (float)(g.step_len + (long)g.step_space);
        maxFreq[i] = (uint32_t)(timing < cap ? timing : cap);

        // Now for this axis: the planning of the axes before it took time
        const int64_t nowT = (int64_t)((dwt64() - startDwt[i]) / ratio);
        const int64_t lead = (ps.open ? ps.openStart : ps.qEnd) - nowT;
        if (lead < leadMin) leadMin = lead < -0x7FFFFFFF ? -0x7FFFFFFF : (int32_t)lead;

        stepAxisCycle(&g, axis[i], ps, qq, tm[i], k, nowT, cycleTicks, cap);
    }
    planCycles = DWT->CYCCNT - t0;
}

StepDiagnostics StepTimers::takeDiagnostics()
{
    StepDiagnostics d;
    __disable_irq();
    d.isrCycles = isrCycles;
    isrCycles = 0;
    d.isrMaxCycles = isrMaxCycles;
    __enable_irq();
    d.planCycles = planCycles;
    d.late = late;
    d.leadMinTicks = leadMin;
    return d;
}

void StepTimers::resetDiagnostics()
{
    isrMaxCycles = 0;
    leadMin = 0x7FFFFFFF;
}
