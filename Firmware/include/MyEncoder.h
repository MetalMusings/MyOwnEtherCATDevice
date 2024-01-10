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
    MyEncoder(uint8_t _indexPin, void irq(void))
    {
        indexPin = _indexPin;
        attachInterrupt(digitalPinToInterrupt(_indexPin), irq, RISING); // When Index triggered
    }

#define ONE_PERIOD 65536
#define HALF_PERIOD 32768

    int32_t unwrap_encoder(uint16_t in)
    {
        int32_t c32 = (int32_t)in - HALF_PERIOD;           // remove half period to determine (+/-) sign of the wrap
        int32_t dif = (c32 - previousEncoderCounterValue); // core concept: prev + (current - prev) = current

        // wrap difference from -HALF_PERIOD to HALF_PERIOD. modulo prevents differences after the wrap from having an incorrect result
        int32_t mod_dif = ((dif + HALF_PERIOD) % ONE_PERIOD) - HALF_PERIOD;
        if (dif < -HALF_PERIOD)
            mod_dif += ONE_PERIOD; // account for mod of negative number behavior in C

        int32_t unwrapped = previousEncoderCounterValue + mod_dif;
        previousEncoderCounterValue = unwrapped; // load previous value

        return unwrapped + HALF_PERIOD; // remove the shift we applied at the beginning, and return
    }

    void indexPulse(void)
    {
        if (pleaseZeroTheCounter)
        {
            TIM2->CNT = 0;
            indexPulseFired = 1;
            Pos.clear();
            TDelta.clear();
            pleaseZeroTheCounter = 0;
        }
    }
    void init(enum EncTimer timer)
    {
        // Set starting count value
        EncoderInit.SetCount(timer, 0);
        // EncoderInit.SetCount(Tim3, 0);
        // EncoderInit.SetCount(Tim4, 0);
        // EncoderInit.SetCount(Tim8, 0);
    }

    uint8_t indexHappened()
    {
        if (indexPulseFired)
        {
            indexPulseFired = 0;
            previousEncoderCounterValue = 0;
            return 1;
        }
        return 0;
    }

    double currentPos()
    {
        curPos = unwrap_encoder(TIM2->CNT) * PosScaleRes;
        return curPos;
    }

    double frequency(uint64_t time)
    {

        double diffT = 0;
        double diffPos = 0;
        TDelta.push(time); // Running average over the length of the circular buffer
        Pos.push(curPos);
        if (Pos.size() >= 2)
        {
            diffT = 1.0e-9 * (TDelta.last() - TDelta.first()); // Time is in nanoseconds
            diffPos = fabs(Pos.last() - Pos.first());
        }
        return diffT != 0 ? diffPos / diffT : 0.0; // Revolutions per second
    }
    uint8_t getIndexState()
    {
        return digitalRead(indexPin);
    }
};

#endif
