
#ifndef STEPGEN
#define STEPGEN
#include <HardwareTimer.h>

class StepGen2
{
private:
    volatile double_t actualPosition;
    volatile int32_t nSteps;
    volatile uint32_t timerPulseSteps;
    volatile float Tstart;
    const float maxAllowedFrequency = 100000; // 100 kHz for now
    HardwareTimer *pulseTimer;
    uint32_t pulseTimerChan;
    HardwareTimer *startTimer; // 10,11,13,14
    uint8_t dirPin;
    PinName stepPin;
    const float Tjitter = 50.0; // Time unit is microseconds

public:
    volatile double_t commandedPosition;    // End position when this cycle is completed
    volatile int32_t commandedStepPosition; // End step position when this cycle is completed
    volatile double_t initialPosition;      // From previous cycle
    volatile int32_t initialStepPosition;   // From previous cycle
    int16_t stepsPerMM;                     // This many steps per mm
    volatile uint8_t enabled;               // Enabled step generator
    volatile float frequency;

    static uint32_t sync0CycleTime;  // Nominal EtherCAT cycle time
    volatile uint32_t lcncCycleTime; // Linuxcnc nominal cycle time (1 ms often)

    StepGen2(TIM_TypeDef *Timer, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void), TIM_TypeDef *Timer2, void irq2(void));

    uint32_t handleStepper(void);
    void startTimerCB();
    void pulseTimerCB();
    uint32_t updatePos(uint32_t i);
};

#endif