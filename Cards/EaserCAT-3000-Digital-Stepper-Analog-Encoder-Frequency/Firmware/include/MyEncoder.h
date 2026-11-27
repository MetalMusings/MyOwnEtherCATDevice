#ifndef MYENCODER
#define MYENCODER
#include "Stm32F4_Encoder.h"
#include <CircularBuffer.hpp>
#define RINGBUFFERLEN 11

class MyEncoder
{
public:
    MyEncoder(TIM_TypeDef *_tim_base, uint8_t _indexPin, void irq(void));
    int64_t unwrapEncoder(uint16_t in);
    void indexPulse(void);
    uint8_t indexHappened();
    float currentPos();
    float frequency(uint64_t time);
    uint8_t getIndexState();
    void setScale(float scale);
    void setLatch(uint8_t latchEnable);

private:
    int64_t previousEncoderCounterValue = 0;
    float PosScaleRes = 1.0f;
    float CurPosScale = 1.0f;
    uint8_t oldLatchCEnable = 0;
    volatile uint8_t indexPulseFired = 0;
    volatile uint8_t pleaseZeroTheCounter = 0;
    Encoder EncoderInit;
    uint8_t indexPin;

    CircularBuffer<int64_t, RINGBUFFERLEN> Pos;  // counts, scaled when used
    CircularBuffer<uint64_t, RINGBUFFERLEN> TDelta; // micros(), extended to 64 bit
    int64_t curCount = 0;
    float oldFrequency = 0.0f;

    TIM_TypeDef *tim_base;
};

#endif
