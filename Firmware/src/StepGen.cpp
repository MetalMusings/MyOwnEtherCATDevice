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

    /*
            TIM_TypeDef *Timer = TIM1;
            MyTim = new HardwareTimer(Timer);
            MyTim->setMode(4, TIMER_OUTPUT_COMPARE_PWM2, STEPPER_STEP_PIN);
            MyTim->attachInterrupt(TimerStep_CB);
            pinMode(STEPPER_DIR_PIN, OUTPUT);
      */
}
void StepGen::cmdPos(double_t pos)
{
    requestedPosition = pos;
}

double StepGen::actPos()
{
    return actualPosition;
}
double StepGen::reqPos()
{
    return requestedPosition;
}
void StepGen::handleStepper(void)
{
    actualPosition = timerStepPosition / double(stepsPerMM);
    double diffPosition = requestedPosition - actualPosition;

    uint64_t fre = abs(diffPosition) * stepsPerMM * 1000000 / double(sync0CycleTime); // Frequency needed
    if (fre > maxFreq)                                                                // Only do maxFre
    {
        double maxDist = maxFreq/stepsPerMM * sync0CycleTime / 1000000.0 * (diffPosition > 0 ? 1 : -1);
        requestedPosition = actualPosition + maxDist;
    }
    int32_t pulsesAtEndOfCycle = stepsPerMM * requestedPosition; // From Turner.hal X:5000 Z:2000 ps/mm
    makePulses(sync0CycleTime, pulsesAtEndOfCycle);              // Make the pulses using hardware timer
}
void StepGen::setCycleTime(uint32_t cycleTime)
{
    sync0CycleTime = cycleTime;
}

void StepGen::makePulses(uint64_t cycleTime /* in usecs */, int32_t pulsesAtEnd /* end position*/)
{
    if (timerIsRunning)
    {
        // Set variables, they will be picked up by the timer_CB and the timer is reloaded.
        timerNewEndStepPosition = pulsesAtEnd;
        timerNewCycleTime = cycleTime;
    }
    if (!timerIsRunning)
    {
        // Start the timer
        int32_t steps = pulsesAtEnd - timerStepPositionAtEnd; // Pulses to go + or -
        if (steps != 0)
        {
            uint8_t sgn = steps > 0 ? HIGH : LOW;
            digitalWrite(dirPin, sgn);
            double_t freqf = (abs(steps) * 1000000.0) / double(cycleTime);
            uint32_t freq = uint32_t(freqf);
            // freq=1428;
            MyTim->setOverflow(freq, HERTZ_FORMAT);
            MyTim->setCaptureCompare(timerChan, 50, PERCENT_COMPARE_FORMAT); // 50 %
            timerStepDirection = steps > 0 ? 1 : -1;
            timerStepPositionAtEnd = pulsesAtEnd; // Current Position
            timerIsRunning = 1;
            MyTim->setMode(timerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
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
        MyTim->pause();
        int32_t steps = timerNewEndStepPosition - timerStepPosition;
        if (steps != 0)
        {
            uint8_t sgn = steps > 0 ? HIGH : LOW;
            digitalWrite(stepPin, sgn);
            double_t freqf = (abs(steps) * 1000000.0) / double(timerNewCycleTime);
            uint32_t freq = uint32_t(freqf);
            // freq=1428;
            if (freq != 0)
            {
                MyTim->setMode(timerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
                // freq=1428;
                MyTim->setOverflow(freq, HERTZ_FORMAT);
                MyTim->setCaptureCompare(timerChan, 50, PERCENT_COMPARE_FORMAT); // 50 %
                timerStepDirection = steps > 0 ? 1 : -1;
                timerStepPositionAtEnd = timerNewEndStepPosition;
                timerNewEndStepPosition = 0; // Set to zero to not reload next time
                timerNewCycleTime = 0;
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
