#ifdef STEPPER_TEST
#include <Arduino.h>
#include <stdio.h>

HardwareSerial Serial1(PA10, PA9);

#include "StepGen3.h"
StepGen3 *Step = 0;
int baseTimer;                     // The base period timer
int syncTimer;                     // The timer that syncs "with linuxcnc cycle"
uint32_t sync0CycleTime = 1000000; // nanosecs, often 1000000 ( 1 ms )
volatile double posCmd1, posCmd2, posCmd3, posCmd4;
volatile uint16_t basePeriodCnt;
volatile uint16_t deltaMakePulsesCnt;
volatile uint64_t makePulsesCnt = 0, prevMakePulsesCnt = 0;
void syncWithLCNC(void);
void basePeriodCB(void);

void setup(void)
{
    Serial1.begin(115200);
    delay(1000);

    Step = new StepGen3;
    pinMode(PA11, OUTPUT); // Step 1
    pinMode(PA12, OUTPUT); // Dir 1
    pinMode(PC9, OUTPUT);  // Step 2
    pinMode(PC8, OUTPUT);  // Dir 2
    pinMode(PD12, OUTPUT); // Step 3
    pinMode(PD11, OUTPUT); // Dir 3
    pinMode(PE5, OUTPUT);  // Step 4
    pinMode(PE4, OUTPUT);  // Dir 4

    posCmd1 = 1.0; // The position update
    posCmd2 = 2.0;
    posCmd3 = 3.0;
    posCmd4 = 4.0;

    for (int k = 0; k < 50; k++)
    {                                                                            // Every ms
        Step->updateStepGen(posCmd1, posCmd2, posCmd3, posCmd4, sync0CycleTime); // Update positions, pos_fb etc
        Serial1.print(" pos_fb = ");
        Serial1.print(Step->stepgen_array[0].pos_fb);
        Serial1.print(" ");
        Serial1.print(Step->stepgen_array[1].pos_fb);
        Serial1.print(" ");
        Serial1.print(Step->stepgen_array[2].pos_fb);
        Serial1.print(" ");
        Serial1.println(Step->stepgen_array[3].pos_fb);

        Step->makeAllPulses();                        // Make first step right here
        basePeriodCnt = sync0CycleTime / BASE_PERIOD; // 50 counts
        for (int i = 0; i < 50; i++)
        {
            if (--basePeriodCnt > 0)   // Stop
                Step->makeAllPulses(); // Only make pulses
        }
    }
}
// baseTimer runs every 20 usecs => 50 kHz. Every ms it runs 50 times
// syncTimer runs ever 1 ms
void loop(void)
{
}

void syncWithLCNC(void)
{
    Step->updateStepGen(posCmd1, posCmd2, posCmd3, posCmd4, sync0CycleTime); // Update positions
    Step->makeAllPulses();                                                   // Make first step right here
    basePeriodCnt = sync0CycleTime / BASE_PERIOD;                            // 50 counts
}

void basePeriodCB(void)
{
    if (--basePeriodCnt > 0) // Stop
        Step->makeAllPulses();
}
#endif