
#ifndef STEPGEN3
#define STEPGEN3
#include <HardwareTimer.h>

class StepGen3
{
public:
    volatile double_t actualPosition;
    volatile int32_t nSteps;
    volatile uint32_t timerFrequency;
    volatile int32_t timerPosition = 0;
    volatile int32_t timerEndPosition = 0;

public:
    volatile float Tstartf;    // Starting delay in secs
    volatile uint32_t Tstartu; // Starting delay in usecs
    volatile float Tpulses;    // Time it takes to do pulses. Debug

    HardwareTimer *pulseTimer;
    uint32_t pulseTimerChan;
    HardwareTimer *startTimer; // Use timers 10,11,13,14
    uint8_t dirPin;
    PinName dirPinName;
    PinName stepPin;
    uint32_t Tjitter = 400; // Longest time from IRQ to handling in handleStepper, unit is microseconds
    uint64_t dbg;
    const uint16_t t2 = 5;                                            // DIR is ahead of PUL with at least 5 usecs
    const uint16_t t3 = 5;                                            // Pulse width at least 2.5 usecs
    const uint16_t t4 = 5;                                            // Low level width not less than 2.5 usecs
    const float maxAllowedFrequency = 1000000 / float(t3 + t4) * 0.9; // 150 kHz for now

public:
    volatile double_t commandedPosition;    // End position when this cycle is completed
    volatile int32_t commandedStepPosition; // End step position when this cycle is completed
    volatile double_t initialPosition;      // From previous cycle
    volatile int32_t initialStepPosition;   // From previous cycle
    int16_t stepsPerMM;                     // This many steps per mm
    volatile uint8_t enabled;               // Enabled step generator
    volatile float frequency;

    static uint32_t sync0CycleTime; // Nominal EtherCAT cycle time nanoseconds
    volatile float lcncCycleTime;   // Linuxcnc nominal cycle time in sec (1 ms often)

    StepGen3(TIM_TypeDef *Timer, uint32_t _timerChannel, PinName _stepPin, uint8_t _dirPin, void irq(void), TIM_TypeDef *Timer2, void irq2(void));

    uint32_t handleStepper(uint64_t irqTime /* time when irq happened nanosecs */, uint16_t nLoops);
    void startTimerCB();
    void pulseTimerCB();
    uint32_t updatePos(uint32_t i);
};

#endif