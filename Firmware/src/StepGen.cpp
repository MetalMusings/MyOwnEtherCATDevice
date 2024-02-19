#include <Arduino.h>
#include <stdio.h>
#include "StepGen.h"

StepGen::StepGen(TIM_TypeDef *Timer, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void))
{
    timerIsRunning = 0;
    timerStepPosition = 0;
    timerStepDirection = 0;
    timerStepPositionAtEnd = 0;
    timerNewEndStepPosition = 0;
    actualPosition = 0;
    requestedPosition = 0;
    stepsPerMM = 0;
    enabled = 0;

    dirPin = _dirPin;
    stepPin = _stepPin;
    timerChan = _timerChannel;
    MyTim = new HardwareTimer(Timer);
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

void StepGen::enable(uint8_t yes)
{
    enabled = yes;
}

void StepGen::handleStepper(void)
{
    if (!enabled)
        return;
    pwmCycleTime = StepGen::sync0CycleTime;

    actPos(timerStepPosition / double(stepsPerMM));
    double diffPosition = reqPos() - actPos();
#if 1
// Wild "tone" kludge. map() function
#define SPEED_MIN 0.00005
#define SPEED_MAX 0.0005
#define FACT_LOW 1.0
#define FACT_HIGH 20.0
    if (abs(diffPosition) < SPEED_MIN) // 60 mm/min = 0.001 mm/ms
    {
        pwmCycleTime = FACT_LOW * StepGen::sync0CycleTime;
    }
    else if (abs(diffPosition) > SPEED_MAX) // 60 mm/min = 0.001 mm/ms
    {
        pwmCycleTime = FACT_HIGH * StepGen::sync0CycleTime;
    }
    else
    {
        pwmCycleTime = (FACT_LOW + (FACT_HIGH - FACT_LOW) * (abs(diffPosition) - SPEED_MIN) / (SPEED_MAX - SPEED_MIN)) * StepGen::sync0CycleTime;
    }
#endif
    uint64_t fre = (abs(diffPosition) * stepsPerMM * 1000000) / pwmCycleTime; // Frequency needed
    if (fre > maxFreq)                                                        // Only do maxFre
    {
        double maxDist = (maxFreq * pwmCycleTime) / (stepsPerMM * 1000000.0) * (diffPosition > 0 ? 1 : -1);
        reqPos(actPos() + maxDist);
    }
    int32_t pulsesAtEndOfCycle = stepsPerMM * reqPos();

    // Will be picked up by the timer_CB and the timer is reloaded, if it runs.
    timerNewEndStepPosition = pulsesAtEndOfCycle;

    if (!timerIsRunning) // Timer isn't running. Start it here
    {
        int32_t steps = pulsesAtEndOfCycle - timerStepPosition; // Pulses to go + or -
        if (steps != 0)
        {
            if (steps > 0)
            {
                digitalWrite(dirPin, HIGH);
                timerStepDirection = 1;
            }
            else
            {
                digitalWrite(dirPin, LOW);
                timerStepDirection = -1;
            }
            timerStepPositionAtEnd = pulsesAtEndOfCycle; // Current Position
            float_t freqf = abs(steps) / (pwmCycleTime*1.0e-6);
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
    if (timerNewEndStepPosition != 0)        // Are we going to reload?
    {
        // Input for reload is timerNewEndStepPosition
        // The timer has current position and from this
        // can set new frequency and new endtarget for steps
        MyTim->pause(); // We are not at stop, let's stop it. Note stepPin is floating
        int32_t steps = timerNewEndStepPosition - timerStepPosition;
        if (steps != 0)
        {
            uint8_t sgn = steps > 0 ? HIGH : LOW;
            digitalWrite(dirPin, sgn);
            float_t freqf = abs(steps) / float(pwmCycleTime*1.0e-6);
            uint32_t freq = uint32_t(freqf);
            timerStepDirection = steps > 0 ? 1 : -1;
            timerStepPositionAtEnd = timerNewEndStepPosition;
            timerNewEndStepPosition = 0; // Set to zero to not reload next time
            MyTim->setMode(timerChan, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
            MyTim->setOverflow(freq, HERTZ_FORMAT);
            MyTim->setCaptureCompare(timerChan, 50, PERCENT_COMPARE_FORMAT); // 50 %
            MyTim->resume();
            timerIsRunning = 1;
        }
    }
    if (timerStepPosition == timerStepPositionAtEnd) // Are we finished?
    {
        timerIsRunning = 0;
        MyTim->pause();
    }
}

void StepGen::setScale(int16_t spm)
{
    stepsPerMM = spm;
}

uint32_t StepGen::sync0CycleTime = 0;
