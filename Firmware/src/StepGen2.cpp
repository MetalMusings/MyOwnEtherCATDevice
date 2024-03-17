#include <Arduino.h>
#include <stdio.h>
#include "StepGen2.h"
extern "C"
{
#include "esc.h"
}
extern extend32to64 longTime;

StepGen2::StepGen2(TIM_TypeDef *Timer, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void), TIM_TypeDef *Timer2, void irq2(void))
{
    actualPosition = 0;
    commandedPosition = 0;
    commandedStepPosition = 0;
    initialPosition = 0;
    initialStepPosition = 0;
    stepsPerMM = 0;
    enabled = 0;

    dirPin = _dirPin;
    stepPin = _stepPin;
    pulseTimerChan = _timerChannel;
    pulseTimer = new HardwareTimer(Timer);
    pulseTimer->attachInterrupt(pulseTimerChan, irq); // Capture/compare innterrupt
    pinMode(dirPin, OUTPUT);
    startTimer = new HardwareTimer(Timer2);
    startTimer->attachInterrupt(irq2);
}

uint32_t StepGen2::handleStepper(uint64_t irqTime, uint16_t nLoops)
{
    frequency = 0;
    nSteps = 0;
    dbg = 0;
    if (!enabled) // Just .... don't
        return updatePos(0);

    commandedStepPosition = floor(commandedPosition * stepsPerMM); // Scale position to steps
#if 0
    if (initialStepPosition == commandedStepPosition)              // No movement
    {
        return updatePos(1);
    }
#endif
    nSteps = commandedStepPosition - initialStepPosition;
    lcncCycleTime = nLoops * StepGen2::sync0CycleTime * 1.0e-9; // nLoops is there in case we missed an ethercat cycle. secs

    if (abs(nSteps) < 0)                                                                    // Some small number
    {                                                                                       //
        frequency = (abs(nSteps) + 1) / lcncCycleTime;                                      // Distribute steps inside available time
        Tpulses = abs(nSteps) / frequency;                                                  //
        Tstartf = (lcncCycleTime - Tpulses) / 2.0;                                          //
    }                                                                                       //
    else                                                                                    // Regular step train, up or down
    {                                                                                       //
        float kTRAJ = (commandedPosition - initialPosition) / lcncCycleTime;                // Straight line equation.  position = kTRAJ x time + mTRAJ
        float mTRAJ = initialPosition;                                                      // Operating on incoming positions (not steps)
        if (kTRAJ > 0)                                                                      //
            Tstartf = (float(initialStepPosition + 1) / float(stepsPerMM) - mTRAJ) / kTRAJ; // Crossing upwards
        else                                                                                //
            Tstartf = (float(initialStepPosition) / float(stepsPerMM) - mTRAJ) / kTRAJ;     // Crossing downwards
        frequency = fabs(kTRAJ * stepsPerMM);                                               //
        Tpulses = abs(nSteps) / frequency;
    }
    updatePos(5);

    uint32_t timeSinceISR = (longTime.extendTime(micros()) - irqTime); // Diff time from ISR (usecs)
    dbg = timeSinceISR;                                                //
    Tstartu = Tjitter + uint32_t(Tstartf * 1e6) - timeSinceISR;        // Have already wasted some time since the irq.

    if (nSteps == 0) // Can do this much earlier, but want some calculated data for debugging
        return updatePos(1);

    timerFrequency = uint32_t(ceil(frequency));
    startTimer->setOverflow(Tstartu, MICROSEC_FORMAT); // All handled by irqs
    startTimer->refresh();
    startTimer->resume();
    return 1;
}

void StepGen2::startTimerCB()
{
    startTimer->pause();                                                    // Once is enough.
    digitalWriteFast(digitalPinToPinName(dirPin), nSteps < 0 ? HIGH : LOW); // nSteps negative => decrease, HIGH
                                                                            // There will be a short break here for t2 usecs, in the future.
    timerEndPosition += nSteps;
    pulseTimer->pause();
    pulseTimer->setMode(pulseTimerChan, TIMER_OUTPUT_COMPARE_PWM1, stepPin);
    pulseTimer->setOverflow(timerFrequency, HERTZ_FORMAT);
    pulseTimer->setCaptureCompare(pulseTimerChan, 50, PERCENT_COMPARE_FORMAT);
    pulseTimer->refresh();
    pulseTimer->resume();
}

void StepGen2::pulseTimerCB()
{
    int16_t dir = digitalReadFast(digitalPinToPinName(dirPin));
    if (dir == HIGH)
        timerPosition--;
    else
        timerPosition++;
    int32_t diffPosition = timerEndPosition - timerPosition; // Same "polarity" as nSteps
    if (diffPosition == 0)
        pulseTimer->pause();
    else
    {
        if (diffPosition < 0 && dir == LOW)                      // Change direction. Should not end up here, but alas
            digitalWriteFast(digitalPinToPinName(dirPin), HIGH); // Normal is to be HIGH when decreasing
        if (diffPosition > 0 && dir == HIGH)                     // Change direction. Should not end up here, but alas
            digitalWriteFast(digitalPinToPinName(dirPin), LOW);  // Normal is to be LOW when increasing
                                                                 // Normally nothing is needed
    }
}

uint32_t StepGen2::updatePos(uint32_t i)
{                                                //
    initialPosition = commandedPosition;         // Save the numeric position for next step
    initialStepPosition = commandedStepPosition; // also the step we are at}
    return i;
}

uint32_t StepGen2::sync0CycleTime = 0;

// Extend from 32-bit to 64-bit precision
int64_t extend32to64::extendTime(uint32_t in)
{
    int64_t c64 = (int64_t)in - HALF_PERIOD; // remove half period to determine (+/-) sign of the wrap
    int64_t dif = (c64 - previousTimeValue); // core concept: prev + (current - prev) = current

    // wrap difference from -HALF_PERIOD to HALF_PERIOD. modulo prevents differences after the wrap from having an incorrect result
    int64_t mod_dif = ((dif + HALF_PERIOD) % ONE_PERIOD) - HALF_PERIOD;
    if (dif < int64_t(-HALF_PERIOD))
        mod_dif += ONE_PERIOD; // account for mod of negative number behavior in C

    int64_t unwrapped = previousTimeValue + mod_dif;
    previousTimeValue = unwrapped; // load previous value

    return unwrapped + HALF_PERIOD; // remove the shift we applied at the beginning, and return
}