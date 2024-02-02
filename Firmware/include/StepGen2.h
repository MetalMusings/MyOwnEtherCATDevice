
#ifndef STEPGEN
#define STEPGEN
#include <HardwareTimer.h>

class StepGen2
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
    volatile double_t oldPosition;
    volatile int32_t oldStepPosition;
    volatile uint8_t enabled;
    HardwareTimer *MyTim;
    HardwareTimer *MyTim2;
    int16_t stepsPerMM;
    uint8_t dirPin;
    PinName stepPin;
    uint32_t timerChan;
    const uint32_t maxFreq = 100000;
    volatile uint32_t prevFreq1 = 0;
    volatile uint32_t prevFreq2 = 0;
    const float Tjitter = 50.0; // Time unit is microseconds

    uint32_t err = 0;

public:
    static uint32_t sync0CycleTime;
    volatile uint32_t lcncCycleTime; // Linuxcnc nominal cycle time (1 ms often)

    StepGen2(TIM_TypeDef *Timer, TIM_TypeDef *Timer2, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void));

    uint32_t handleStepper(void);
    void timerCB();
    void enable(uint8_t yes);

    void reqPos(double_t pos) { requestedPosition = pos; };
    double reqPos() { return requestedPosition; };
    void oldPos(double_t pos) { oldPosition = pos; };
    double oldPos() { return oldPosition; };
    void oldStepPos(int32_t pos) { oldStepPosition = pos; }
    int32_t oldStepPos() { return oldStepPosition; }
    void actPos(double_t pos) { actualPosition = pos; };
    double actPos() { return actualPosition; };
    void setScale(int16_t spm) { stepsPerMM = spm; }
    int16_t getScale() { return stepsPerMM; }
    uint32_t updatePosAndReturn(int32_t stepPosStop, uint32_t i);
};

#endif