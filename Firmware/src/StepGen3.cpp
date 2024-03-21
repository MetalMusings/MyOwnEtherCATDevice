#include <Arduino.h>
#include <stdio.h>
#include "StepGen3.h"
#include "extend32to64.h"

extern "C"
{
#include "esc.h"
}
extern extend32to64 longTime;

StepGen3::StepGen3(TIM_TypeDef *Timer, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void), TIM_TypeDef *Timer2, void irq2(void))
{
    actualPosition = 0;
    commandedPosition = 0;
    commandedStepPosition = 0;
    initialPosition = 0;
    initialStepPosition = 0;
    stepsPerMM = 0;
    enabled = 0;

    dirPin = _dirPin;
    dirPinName = digitalPinToPinName(dirPin);
    stepPin = _stepPin;
    pulseTimerChan = _timerChannel;
    pulseTimer = new HardwareTimer(Timer);
    pulseTimer->attachInterrupt(pulseTimerChan, irq); // Capture/compare innterrupt
    pinMode(dirPin, OUTPUT);
    startTimer = new HardwareTimer(Timer2);
    startTimer->attachInterrupt(irq2);
}

uint32_t StepGen3::handleStepper(uint64_t irqTime, uint16_t nLoops)
{
    frequency = 0;
    nSteps = 0;
    dbg = 0;
    if (!enabled) // Just .... don't
        return updatePos(0);

    commandedStepPosition = floor(commandedPosition * stepsPerMM); // Scale position to steps
    nSteps = commandedStepPosition - initialStepPosition;
    if (nSteps == 0) // No movement
    {
        return updatePos(1);
    }
    lcncCycleTime = nLoops * StepGen3::sync0CycleTime * 1.0e-9; // nLoops is there in case we missed an ethercat cycle. secs

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

void StepGen3::startTimerCB()
{
    startTimer->pause();                                   // Once is enough.
    digitalWriteFast(dirPinName, nSteps < 0 ? HIGH : LOW); // nSteps negative => decrease, HIGH
                                                           // There will be a short break here for t2 usecs, in the future.

    timerEndPosition += nSteps;
    pulseTimer->setMode(pulseTimerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
    pulseTimer->setOverflow(timerFrequency, HERTZ_FORMAT);
    // pulseTimer->setCaptureCompare(pulseTimerChan, 50, PERCENT_COMPARE_FORMAT);
    pulseTimer->setCaptureCompare(pulseTimerChan, t3, MICROSEC_COMPARE_FORMAT);
    pulseTimer->refresh();
    pulseTimer->resume();
}

void StepGen3::pulseTimerCB()
{
    int16_t dir = digitalReadFast(dirPinName); //
    if (dir == HIGH)                           // The step just taken
        timerPosition--;
    else
        timerPosition++;
    int32_t diffPosition = timerEndPosition - timerPosition; // Same "polarity" as nSteps
    if (diffPosition == 0)
        pulseTimer->pause();
    else
    {
        if (diffPosition < 0 && dir == LOW)     // Change direction. Should not end up here, but alas
            digitalWriteFast(dirPinName, HIGH); // Normal is to be HIGH when decreasing
        if (diffPosition > 0 && dir == HIGH)    // Change direction. Should not end up here, but alas
            digitalWriteFast(dirPinName, LOW);  // Normal is to be LOW when increasing
                                                // Normally nothing is needed
    }
}

uint32_t StepGen3::updatePos(uint32_t i)
{                                                //
    initialPosition = commandedPosition;         // Save the numeric position for next step
    initialStepPosition = commandedStepPosition; // also the step we are at}
    return i;
}

uint32_t StepGen3::sync0CycleTime = 0;

