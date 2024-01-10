#ifndef MYENCODER
#define MYENCODER
#include "Stm32F4_Encoder.h"
#include <CircularBuffer.h>
#define RINGBUFFERLEN 101
   //  EncoderInit.SetCount(Tim3, 0);
   //  EncoderInit.SetCount(Tim4, 0);
   //  EncoderInit.SetCount(Tim8, 0);
class MyEncoder
{
public:
    int32_t previousEncoderCounterValue = 0;
    double PosScaleRes = 1.0;
    uint32_t CurPosScale = 1;
    uint8_t OldLatchCEnable = 0;
    volatile uint8_t indexPulseFired = 0;
    volatile uint8_t pleaseZeroTheCounter = 0;
    Encoder EncoderInit;
    uint8_t indexPin;

    CircularBuffer<double_t, RINGBUFFERLEN> Pos;
    CircularBuffer<uint32_t, RINGBUFFERLEN> TDelta;
    double curPos;

public:
    MyEncoder(uint8_t _indexPin, void irq(void));
    int32_t unwrapEncoder(uint16_t in);
    void indexPulse(void);
    void init(enum EncTimer timer);
    uint8_t indexHappened();
    double currentPos();
    double frequency(uint64_t time);
    uint8_t getIndexState();
};

#endif
