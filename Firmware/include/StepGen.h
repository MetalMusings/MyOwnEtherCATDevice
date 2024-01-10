
#ifndef STEPGEN
#define STEPGEN
#include <HardwareTimer.h>

class StepGen
{
public:
    volatile uint8_t timerIsRunning;
    volatile int32_t timerStepPosition;
    volatile int32_t timerStepDirection;
    volatile int32_t timerStepPositionAtEnd;
    volatile int32_t timerNewEndStepPosition;
    volatile uint32_t timerNewCycleTime;
    volatile double_t actualPosition;
    volatile double_t requestedPosition;
    HardwareTimer *MyTim;
    const uint32_t stepsPerMM = 1000;
    uint32_t sync0CycleTime;

    StepGen(TIM_TypeDef *Timer, uint8_t timerChannel, uint8_t stepPin, uint8_t dirPin, void irq(void))
    {
        timerIsRunning = 0;
        timerStepPosition = 0;
        timerStepDirection = 0;
        timerStepPositionAtEnd = 0;
        timerNewEndStepPosition = 0;
        timerNewCycleTime = 0;
        actualPosition = 0;
        requestedPosition = 0;

        MyTim = new HardwareTimer(Timer);
        MyTim->setMode(timerChannel, TIMER_OUTPUT_COMPARE_PWM2, stepPin);
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

    void cmdPos(double_t pos)
    {
        requestedPosition = pos;
    }

    double actPos()
    {
        return actualPosition;
    }
    double reqPos()
    {
        return requestedPosition;
    }
    void handleStepper(void)
    {
        actualPosition = timerStepPosition / double(stepsPerMM);
        double diffPosition = requestedPosition - actualPosition;
        if (abs(diffPosition) * stepsPerMM > 10000)
        {
            requestedPosition = actualPosition + 10.0 * (diffPosition > 0 ? 1 : -1);
        }
        int32_t pulsesAtEndOfCycle = stepsPerMM * requestedPosition; // From Turner.hal X:5000 Z:2000 ps/mm
        makePulses(sync0CycleTime, pulsesAtEndOfCycle);              // Make the pulses using hardware timer
    }
    void setCycleTime(uint32_t cycleTime)
    {
        sync0CycleTime = cycleTime;
    }

    void makePulses(uint64_t cycleTime /* in usecs */, int32_t pulsesAtEnd /* end position*/)
    {
        uint32_t now = micros();
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
                digitalWrite(STEPPER_DIR_PIN, sgn);
                double_t freqf = (abs(steps) * 1000000.0) / double(cycleTime);
                uint32_t freq = uint32_t(freqf);
                // freq=1428;
                MyTim->setOverflow(freq, HERTZ_FORMAT);
                MyTim->setCaptureCompare(4, 50, PERCENT_COMPARE_FORMAT); // 50 %
                timerStepDirection = steps > 0 ? 1 : -1;
                timerStepPositionAtEnd = pulsesAtEnd; // Current Position
                timerIsRunning = 1;
                MyTim->setMode(4, TIMER_OUTPUT_COMPARE_PWM2, STEPPER_STEP_PIN);
                MyTim->resume();
            }
        }
    }
    void timerCB()
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
                digitalWrite(STEPPER_DIR_PIN, sgn);
                double_t freqf = (abs(steps) * 1000000.0) / double(timerNewCycleTime);
                uint32_t freq = uint32_t(freqf);
                // freq=1428;
                if (freq != 0)
                {
                    MyTim->setMode(4, TIMER_OUTPUT_COMPARE_PWM2, STEPPER_STEP_PIN);
                    // freq=1428;
                    MyTim->setOverflow(freq, HERTZ_FORMAT);
                    MyTim->setCaptureCompare(4, 50, PERCENT_COMPARE_FORMAT); // 50 %
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
};

#endif