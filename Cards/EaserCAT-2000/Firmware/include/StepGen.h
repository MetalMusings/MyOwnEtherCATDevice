
#ifndef STEPGEN
#define STEPGEN
#include <HardwareTimer.h>

class StepGen
{
private:
    volatile uint8_t timerIsRunning;
    volatile int32_t timerStepPosition;
    volatile int32_t timerStepDirection;
    volatile int32_t timerStepPositionAtEnd;
    volatile int32_t timerNewEndStepPosition;
    volatile uint32_t timerNewCycleTime;
    volatile double_t actualPosition;
    volatile double_t requestedPosition;
    volatile uint8_t enabled;
    HardwareTimer *MyTim;
    HardwareTimer *MyTim2;
    uint16_t stepsPerMM;
    uint8_t dirPin;
    PinName stepPin;
    uint32_t timerChan;
    const uint32_t maxFreq = 100000;
    volatile uint32_t prevFreq1 = 0;
    volatile uint32_t prevFreq2 = 0;

public:
    static uint32_t sync0CycleTime;
    volatile uint32_t pwmCycleTime;

    StepGen(TIM_TypeDef *Timer, uint32_t timerChannel, PinName stepPin, uint8_t dirPin, void irq(void));
    void reqPos(double_t pos);
    double reqPos();
    void actPos(double_t pos);
    double actPos();
    void handleStepper(void);
    void timerCB();
    void setScale(int16_t spm);
    void enable(uint8_t yes);
};

#endif