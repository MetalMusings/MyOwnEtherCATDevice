#include "MyENcoder.h"

MyEncoder::MyEncoder(uint8_t _indexPin, void irq(void))
{
    indexPin = _indexPin;
    attachInterrupt(digitalPinToInterrupt(indexPin), irq, RISING); // When Index triggered
}

#define ONE_PERIOD 65536
#define HALF_PERIOD 32768

int32_t MyEncoder::unwrapEncoder(uint16_t in)
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

void MyEncoder::indexPulse(void)
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
void MyEncoder::init(TIM_TypeDef *_tim_base)
{
    EncoderInit.tim_base = _tim_base;
    // Set starting count value
    EncoderInit.SetCount(0);
    // EncoderInit.SetCount(Tim3, 0);
    // EncoderInit.SetCount(Tim4, 0);
    // EncoderInit.SetCount(Tim8, 0);
}

uint8_t MyEncoder::indexHappened()
{
    if (indexPulseFired)
    {
        indexPulseFired = 0;
        previousEncoderCounterValue = 0;
        return 1;
    }
    return 0;
}

double MyEncoder::currentPos(volatile uint32_t cnt)
{
    curPos = unwrapEncoder(tim_base->CNT) * PosScaleRes;
    return curPos;
}

double MyEncoder::frequency(uint64_t time)
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
uint8_t MyEncoder::getIndexState()
{
    return digitalRead(indexPin);
}

void MyEncoder::setScale(double scale)
{
    if (CurPosScale != scale && scale != 0)
    {
        CurPosScale = scale;
        PosScaleRes = 1.0 / double(scale);
    }
}

void MyEncoder::setLatch(uint8_t latchEnable)
{
    if (latchEnable && !oldLatchCEnable) // Should only happen first time IndexCEnable is set
    {
        pleaseZeroTheCounter = 1;
    }
    oldLatchCEnable = latchEnable;
}

