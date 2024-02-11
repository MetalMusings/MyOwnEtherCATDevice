#include <Arduino.h>
#include <stdio.h>
#include "StepGen2.h"
extern "C"
{
#include "esc.h"
}
extern int64_t extendTime(uint32_t);

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
    pulseTimer->attachInterrupt(irq);
    pinMode(dirPin, OUTPUT);
    startTimer = new HardwareTimer(Timer2);
    startTimer->attachInterrupt(irq2);
}
extern volatile uint32_t cnt;
uint32_t StepGen2::handleStepper(uint64_t irqTime)
{
    digitalWrite(dirPin, cnt++ % 2);
    if (!enabled)
        return updatePos(0);

    lcncCycleTime = StepGen2::sync0CycleTime * 1.0e-6;             // was usec, became sec
    commandedStepPosition = floor(commandedPosition * stepsPerMM); // Scale position to steps
    if (initialStepPosition == commandedStepPosition)              // No movement
        return 1;

    float approximateFrequency = fabs(initialStepPosition - commandedStepPosition) // We must take at least one step
                                 / lcncCycleTime;                                  // from here on
                                                                                   //   if (approximateFrequency > maxAllowedFrequency)                                // Stay on this position
                                                                                   //       return 1;

    float kTRAJ = (commandedPosition - initialPosition) / lcncCycleTime; // Straight line equation
    float mTRAJ = initialPosition;                                       // position = kTRAJ x time + mTRAJ
                                                                         // Operating on incoming positions (not steps)
                                                                         //   if (fabs(kTRAJ * lcncCycleTime * stepsPerMM) < 0.01)                 // Very flat slope
    nSteps = commandedStepPosition - initialStepPosition;                //
    if (abs(nSteps) <= 8)                                                // Some small number
    {                                                                    //
        Tstartf = 0;                                                     // Just take a step in the middle of the cycle
        frequency = 1000 * (abs(nSteps) + 1);                            // At some suitable frequency
    }
    else // Regular step train, up or down
    {
        if (kTRAJ > 0)
            Tstartf = (float(initialStepPosition + 1) / float(stepsPerMM) - mTRAJ) / kTRAJ;
        else
            Tstartf = (float(initialStepPosition) / float(stepsPerMM) - mTRAJ) / kTRAJ;
        frequency = fabs(kTRAJ * stepsPerMM);
        nSteps = commandedStepPosition - initialStepPosition; // sign(nSteps) = direction.
    }
    updatePos(5);
    uint64_t nowTime = extendTime(micros()); // usecs
    dbg = nowTime - irqTime;
    Tstartu = Tjitter + Tstartf * 1e6 // Was secs, now usecs
              - (nowTime - irqTime);  // Have already wasted some time since the irq.

    startTimer->setOverflow(Tstartu, MICROSEC_FORMAT); // All handled by irqs
    startTimer->resume();
    return 1;
}
void StepGen2::startTimerCB()
{
    digitalWrite(dirPin, cnt++ % 2);
    startTimer->pause(); // Once is enough.
    // digitalWrite(dirPin, nSteps > 0 ? 1 : -1);
    timerPulseSteps = abs(nSteps);
    pulseTimer->setMode(pulseTimerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
    pulseTimer->setOverflow(uint32_t(frequency), HERTZ_FORMAT);
    pulseTimer->setCaptureCompare(pulseTimerChan, 50, PERCENT_COMPARE_FORMAT); // 50%
    pulseTimer->resume();
}
void StepGen2::pulseTimerCB()
{
    --timerPulseSteps;
    if (timerPulseSteps == 0)
    {
        pulseTimer->pause();
        digitalWrite(dirPin, cnt++ % 2);
    }
}

uint32_t StepGen2::updatePos(uint32_t i)
{                                                //
    initialPosition = commandedPosition;         // Save the numeric position for next step
    initialStepPosition = commandedStepPosition; // also the step we are at}
    return i;
}

uint32_t StepGen2::sync0CycleTime = 0;
