#include <Arduino.h>
#include <stdio.h>
#include "StepGen2.h"

StepGen2::StepGen2(TIM_TypeDef *Timer, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void))
{
    timerIsRunning = 0;
    timerStepPosition = 0;
    timerStepDirection = 0;
    timerStepPositionAtEnd = 0;
    timerNewEndStepPosition = 0;
    actualPosition = 0;
    requestedPosition = 0;
    oldPosition = 0;
    oldStepPosition = 0;
    stepsPerMM = 0;
    enabled = 0;

    dirPin = _dirPin;
    stepPin = _stepPin;
    timerChan = _timerChannel;
    MyTim = new HardwareTimer(Timer);
    MyTim->attachInterrupt(irq);
    pinMode(dirPin, OUTPUT);
}

uint32_t StepGen2::handleStepper(void)
{
    if (!enabled)
        return 1;
    lcncCycleTime = StepGen2::sync0CycleTime;

    float y0TRAJ = oldPos() * getScale();              // Straight line equation between old and new point
    float y1TRAJ = reqPos() * getScale();              // Time runs between 0 and lcncCycleTime (1 ms)
    float kTRAJ = (y1TRAJ - y0TRAJ) / lcncCycleTime;   // Slope
    float mTRAJ = y1TRAJ - kTRAJ * lcncCycleTime;      // Intercept
    int32_t stepPosStart = floor(y0TRAJ);              // First step position, integer value of first point position
    int32_t stepPosStop = floor(y1TRAJ);               // End step position
                                                       //
    float Tstart = (stepPosStart - mTRAJ) / kTRAJ;     // First step at this time
    float Tstop = (stepPosStop - mTRAJ) / kTRAJ;       // And the last step
    float Tstep = fabs(1.0 / kTRAJ);                   // Time between steps
    float stepFrequency = fabs(kTRAJ);                 // 1/Tstep - which is kTRAJ
                                                       //
    if (Tstart > lcncCycleTime)                        // Not enough movement to make a step
        return updatePosAndReturn(stepPosStop, 2);     //
    if (/* 1.0 / Tstep */ kTRAJ > 200000)              //
    {                                                  // Too high frequency, deal with this later.
        return updatePosAndReturn(stepPosStop, 3);     //
    }                                                  //
    int8_t dir = stepPosStart > stepPosStop ? -1 : 1;  // Which direction to step in
                                                       //
    if (abs(stepPosStart - oldStepPos()) == 0)         // StepPosStart and oldStepPos() are often the same, but don't redo the step
    {                                                  //
        stepPosStart += dir;                           // New first step
        Tstart += Tstep;                               //
        if (Tstart > lcncCycleTime)                    // Not enough movement to make a step
            return updatePosAndReturn(stepPosStop, 4); //
    }                                                  //
    if (abs(stepPosStart - oldStepPos()) > 1)          // Shouldn't happen
    {                                                  //
        return updatePosAndReturn(stepPosStop, 5);     //
    }                                                  //
                                                       // Now the old point and the start point should be separate.
    if (Tstart > lcncCycleTime)                        // Not enough movement to make a step
        return updatePosAndReturn(stepPosStop, 6);     // Check this again
                                                       // Tstart, Tstep and Tstop defines the coming pwm-sequence.
    return 0;                                          // Always do one pulse at Tstart when we come here. Next Tstart+Tstep and so on until Tstop.
}
void StepGen2::timerCB()
{
#if 0
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
            float_t freqf = abs(steps) / float(pwmCycleTime * 1.0e-6);
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
#endif
}
uint32_t StepGen2::updatePosAndReturn(int32_t stepPosStop, uint32_t i)
{                            //
    oldPos(reqPos());        // Save the numeric position for next step
    oldStepPos(stepPosStop); // also the step we are at}
    return i;
}

uint32_t StepGen2::sync0CycleTime = 0;
