#include "FreqCounter.h"

#include <Arduino.h>

// PA2 = TIM5_CH3 (AF2). TIM5_CH3 requests DMA1 stream 0, channel 6 (RM0090 table 42).
void FreqCounter::begin() {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
  (void)RCC->APB1ENR;

  // PA2 alternate function 2, no pull (the MAX3485 drives it)
  GPIOA->MODER = (GPIOA->MODER & ~(3u << (2 * 2))) | (2u << (2 * 2));
  GPIOA->PUPDR &= ~(3u << (2 * 2));
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFu << (2 * 4))) | (2u << (2 * 4));

  // Timer clock: PCLK1, doubled when APB1 is divided
  uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  clkHz = (float)(((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) ? pclk1 : 2 * pclk1);

  TIM5->CR1 = 0;
  TIM5->PSC = 0;
  TIM5->ARR = 0xFFFFFFFF;
  // IC3 on TI3, filter fCK_INT N=8 (ignores pulses shorter than about 0.1 us), no prescaler
  TIM5->CCMR2 = (TIM5->CCMR2 & ~(TIM_CCMR2_CC3S | TIM_CCMR2_IC3F | TIM_CCMR2_IC3PSC)) |
                TIM_CCMR2_CC3S_0 | (3u << TIM_CCMR2_IC3F_Pos);
  TIM5->CCER = (TIM5->CCER & ~(TIM_CCER_CC3P | TIM_CCER_CC3NP)) | TIM_CCER_CC3E;  // rising
  TIM5->EGR = TIM_EGR_UG;

  DMA_Stream_TypeDef *s = DMA1_Stream0;
  s->CR &= ~DMA_SxCR_EN;
  while (s->CR & DMA_SxCR_EN) {
  }
  DMA1->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CHTIF0 |
                DMA_LIFCR_CTCIF0;
  s->PAR = (uint32_t)&TIM5->CCR3;
  s->M0AR = (uint32_t)buf;
  s->NDTR = BUF_LEN;
  s->FCR = 0;  // direct mode
  s->CR = (6u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 | DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1 |
          DMA_SxCR_MINC | DMA_SxCR_CIRC;  // peripheral to memory, 32 bit words
  s->CR |= DMA_SxCR_EN;

  TIM5->DIER |= TIM_DIER_CC3DE;
  TIM5->CR1 |= TIM_CR1_CEN;
  lastPos = 0;
  valid = 0;
  lastEdgeMicros = micros();
}

float FreqCounter::read(uint32_t nowMicros) {
  uint32_t ndtr = DMA1_Stream0->NDTR;
  uint32_t pos = (ndtr == 0 || ndtr > BUF_LEN) ? 0 : (BUF_LEN - ndtr) % BUF_LEN;  // next write
  uint32_t n = (pos + BUF_LEN - lastPos) % BUF_LEN;  // new edges (modulo BUF_LEN)
  lastPos = pos;

  if (n == 0) {
    if (nowMicros - lastEdgeMicros > ZERO_TIMEOUT_US) {
      freq = 0;
      valid = 0;  // the next edges start a new measurement
    }
    return freq;
  }
  lastEdgeMicros = nowMicros;
  valid = (valid + n > BUF_LEN) ? BUF_LEN : valid + n;

  // Periods between the newest edge and the one m edges before it. A wrapped
  // count (BUF_LEN edges or more since the last call) only gives a smaller m:
  // the buffer always holds the most recent edges in order.
  uint32_t m = n;
  if (m > BUF_LEN - MARGIN) m = BUF_LEN - MARGIN;
  if (m > valid - 1) m = valid - 1;
  if (m == 0) return freq;  // the first edge after a restart

  uint32_t t1 = buf[(pos + BUF_LEN - 1) % BUF_LEN];
  uint32_t t0 = buf[(pos + 2 * BUF_LEN - 1 - m) % BUF_LEN];
  uint32_t dt = t1 - t0;
  if (dt != 0) freq = (float)m * clkHz / (float)dt;
  return freq;
}
