#include <Arduino.h>
#include <stdio.h>
#include "StepGen2.h"

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

uint32_t StepGen2::handleStepper(void)
{
    if (!enabled)
        return updatePos(0);
    lcncCycleTime = StepGen2::sync0CycleTime;
    commandedStepPosition = floor(commandedPosition * stepsPerMM); // Scale position to steps
    if (initialStepPosition == commandedStepPosition)              // No movement
        return updatePos(1);

    float approximateFrequency = fabs(initialStepPosition - commandedStepPosition) // We must take at least one step
                                 / (float)lcncCycleTime;                           // from here on
    if (approximateFrequency > maxAllowedFrequency)                                // Stay on this position
        return 1;

    float kTRAJ = (commandedPosition - initialPosition) / float(lcncCycleTime); // Straight line equation
    float mTRAJ = initialPosition;                                              // position = kTRAJ x time + mTRAJ
                                                                                // Operating on incoming positions (not steps)
    if (fabs(kTRAJ * lcncCycleTime * stepsPerMM) < 0.01)                        // Very flat slope
    {                                                                           //
        Tstart = 0.5 * lcncCycleTime;                                           // Just take a step in the middle of the cycle
        frequency = 10000;                                                      // At some suitable frequency
        nSteps = kTRAJ > 0 ? 1 : -1;                                            // Take only one step
    }
    else // Regular step train, up or down
    {
        if (kTRAJ > 0)
            Tstart = (ceil(initialPosition * stepsPerMM) - mTRAJ) / kTRAJ;
        else
            Tstart = (floor(initialPosition * stepsPerMM) - mTRAJ) / kTRAJ;
        frequency = kTRAJ * stepsPerMM;
        nSteps = commandedStepPosition - initialStepPosition; // sign(nSteps) = direction.
    }
    updatePos(5);

    startTimer->setOverflow(Tstart + Tjitter, MICROSEC_FORMAT); // All handled by irqs
    startTimer->resume();
    return 1;
}
void StepGen2::startTimerCB()
{
    startTimer->pause(); // Once is enough.
    digitalWrite(dirPin, nSteps > 0 ? 1 : -1);
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
        pulseTimer->pause();
}

uint32_t StepGen2::updatePos(uint32_t i)
{                                                //
    initialPosition = commandedPosition;         // Save the numeric position for next step
    initialStepPosition = commandedStepPosition; // also the step we are at}
    return i;
}

uint32_t StepGen2::sync0CycleTime = 0;
