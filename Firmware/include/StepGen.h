
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
    uint32_t stepsPerMM;
    static uint32_t sync0CycleTime;
    uint8_t dirPin;
    uint8_t stepPin;
    uint8_t timerChan;
    const uint32_t maxFreq = 100000;

    StepGen(TIM_TypeDef *Timer, uint8_t timerChannel, uint8_t stepPin, uint8_t dirPin, void irq(void));
    void cmdPos(double_t pos);
    double actPos();
    double reqPos();
    void handleStepper(void);
    void makePulses(uint64_t cycleTime /* in usecs */, int32_t pulsesAtEnd /* end position*/);
    void timerCB();
    void setScale(int32_t spm);
};

#endif