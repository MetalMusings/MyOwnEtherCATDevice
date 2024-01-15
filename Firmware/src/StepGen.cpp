#include <Arduino.h>
#include <stdio.h>
#include "StepGen.h"

StepGen::StepGen(TIM_TypeDef *Timer, uint8_t _timerChannel, uint8_t _stepPin, uint8_t _dirPin, void irq(void))
{
    timerIsRunning = 0;
    timerStepPosition = 0;
    timerStepDirection = 0;
    timerStepPositionAtEnd = 0;
    timerNewEndStepPosition = 0;
    timerNewCycleTime = 0;
    actualPosition = 0;
    requestedPosition = 0;
    stepsPerMM = 0;

    dirPin = _dirPin;
    stepPin = _stepPin;
    timerChan = _timerChannel;
    MyTim = new HardwareTimer(Timer);
    MyTim->setMode(timerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
    MyTim->attachInterrupt(irq);
    pinMode(dirPin, OUTPUT);
}
void StepGen::reqPos(double_t pos)
{
    requestedPosition = pos;
}
double StepGen::reqPos()
{
    return requestedPosition;
}
void StepGen::actPos(double pos)
{
    actualPosition = pos;
}
double StepGen::actPos()
{
    return actualPosition;
}

void StepGen::handleStepper(void)
{
    actPos(timerStepPosition / double(stepsPerMM));
    double diffPosition = reqPos() - actPos();

    uint64_t fre = abs(diffPosition) * stepsPerMM * 1000000 / double(sync0CycleTime); // Frequency needed
    if (fre > maxFreq)                                                                // Only do maxFre
    {
        double maxDist = maxFreq / stepsPerMM * sync0CycleTime / 1000000.0 * (diffPosition > 0 ? 1 : -1);
        reqPos(actualPosition + maxDist);
    }
    int32_t pulsesAtEndOfCycle = stepsPerMM * reqPos(); // From Turner.hal X:5000 Z:2000 ps/mm

    sync0CycleTime = 1000;

    if (timerIsRunning)
    {
        // Set variables, they will be picked up by the timer_CB and the timer is reloaded.
        timerNewEndStepPosition = pulsesAtEndOfCycle;
        timerNewCycleTime = sync0CycleTime;
    }
    if (!timerIsRunning)
    {
        // Start the timer
        int32_t steps = pulsesAtEndOfCycle - timerStepPositionAtEnd; // Pulses to go + or -
        if (steps != 0)
        {
            uint8_t sgn = steps > 0 ? HIGH : LOW;
            digitalWrite(dirPin, sgn);
            timerStepDirection = steps > 0 ? 1 : -1;
            timerStepPositionAtEnd = pulsesAtEndOfCycle; // Current Position
            double_t freqf = abs(steps) * (1e6 / double(sync0CycleTime));
            uint32_t freq = uint32_t(freqf);
            MyTim->setMode(timerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
            MyTim->setOverflow(freq, HERTZ_FORMAT);
            MyTim->setCaptureCompare(timerChan, 50, PERCENT_COMPARE_FORMAT); // 50 %
            timerIsRunning = 1;
            MyTim->resume();
        }
    }
}
void StepGen::timerCB()
{
    timerStepPosition += timerStepDirection; // The step that was just completed
    if (timerNewCycleTime != 0)              // Are we going to reload?
    {
        // Input for reload is timerNewEndStepPosition and timerNewEndTime
        // The timer has current position and current time and from this
        // can set new frequency and new endtarget for steps
        MyTim->pause(); // We are not at stop, let's stop it
        int32_t steps = timerNewEndStepPosition - timerStepPosition;
        if (steps != 0)
        {
            uint8_t sgn = steps > 0 ? HIGH : LOW;
            digitalWrite(dirPin, sgn);
            double_t freqf = abs(steps) * (1e6 / double(timerNewCycleTime));
            uint32_t freq = uint32_t(freqf);
            if (freq != 0)
            {
                timerStepDirection = steps > 0 ? 1 : -1;
                timerStepPositionAtEnd = timerNewEndStepPosition;
                timerNewEndStepPosition = 0; // Set to zero to not reload next time
                timerNewCycleTime = 0;
                MyTim->setMode(timerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
                MyTim->setOverflow(freq, HERTZ_FORMAT);
                MyTim->setCaptureCompare(timerChan, 50, PERCENT_COMPARE_FORMAT); // 50 %
                MyTim->resume();
                timerIsRunning = 1;
            }
        }
    }
    if (timerStepPosition == timerStepPositionAtEnd) // Are we finished?
    {
        timerIsRunning = 0;
        MyTim->pause();
    }
}

void StepGen::setScale(int32_t spm)
{
    stepsPerMM = spm;
}

uint32_t StepGen::sync0CycleTime = 0;