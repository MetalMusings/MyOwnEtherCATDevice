#include <Arduino.h>
#include <stdio.h>
extern "C"
{
#include "ecat_slv.h"
#include "utypes.h"
};
_Objects Obj;

HardwareSerial Serial1(PA10, PA9);

volatile uint16_t ALEventIRQ; // ALEvent that caused the interrupt
HardwareTimer *baseTimer;     // The base period timer
HardwareTimer *syncTimer;     // The timer that syncs "with linuxcnc cycle"

#include "MyEncoder.h"
void indexPulseEncoderCB1(void);
MyEncoder Encoder1(TIM2, PA2, indexPulseEncoderCB1);
void indexPulseEncoderCB1(void)
{
   Encoder1.indexPulse();
}
#include <RunningAverage.h>
RunningAverage cycleTimes(1000); // To have a running average of the cycletime over the last second

#include "StepGen3.h"
StepGen3 *Step = 0;

#include "extend32to64.h"

CircularBuffer<uint64_t, 200> Tim;

volatile uint64_t irqTime = 0, thenTime = 0, nowTime = 0, irqCnt = 0, prevSyncTime = 0, syncTime = 0, deltaSyncTime;
extend32to64 longTime;
volatile uint16_t isrTime = 0;
void setFrequencyAdjustedMicrosSeconds(HardwareTimer *timer, uint32_t usecs);

void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   //  Encoder1.setLatch(Obj.IndexLatchEnable);
   //  Encoder1.setScale(2000);

   // Step2.reqPos(Obj.CommandedPosition2);
   // Step2.setScale(Obj.StepsPerMM2);
   // Step2.enable(1);
}

volatile uint16_t basePeriodCnt;
volatile uint64_t makePulsesCnt = 0, prevMakePulsesCnt = 0;
volatile uint16_t deltaMakePulsesCnt;

volatile double pos_cmd1, pos_cmd2;
void syncWithLCNC()
{
   syncTimer->pause();
   prevSyncTime = syncTime;
   syncTime = longTime.extendTime(micros());
   deltaSyncTime = syncTime - prevSyncTime;
   deltaMakePulsesCnt = makePulsesCnt - prevMakePulsesCnt;
   prevMakePulsesCnt = makePulsesCnt;
   Step->updateStepGen(pos_cmd1, pos_cmd2); // Update positions
   Step->makeAllPulses();                   // Make first step right here
   basePeriodCnt = 1000000 / BASE_PERIOD;   //
   baseTimer->setCount(0);
   baseTimer->refresh(); //
   baseTimer->resume();  // Make the other steps in ISR
   // baseTimer->isRunning();
}

void basePeriodCB(void)
{
   uint32_t one = micros();

   Step->makeAllPulses();

   isrTime = micros() - one;
   if (--basePeriodCnt <= 0) // Stop
   {
      baseTimer->pause();
   }
}

uint16_t nLoops;
uint64_t reallyNowTime = 0, reallyThenTime = 0; // Times in microseconds
uint64_t timeDiff;                              // Timediff in microseconds
int32_t delayT;
uint16_t avgCycleTime, thisCycleTime; // In usecs
int16_t jitterThisCycle = 0, maxCycleTime = 0;

volatile uint64_t oldIrqTime = 0;

void handleStepper(void)
{
   if (oldIrqTime != 0)
   {
      thisCycleTime = irqTime - oldIrqTime;
      cycleTimes.add(thisCycleTime);
   }
   oldIrqTime = irqTime;

   if (cycleTimes.bufferIsFull()) // Do max and jitter calcs, just waiting a second
   {
      avgCycleTime = cycleTimes.getFastAverage();
      jitterThisCycle = irqTime - avgCycleTime;
      uint16_t maxInBuffer = cycleTimes.getMaxInBuffer();
      if (maxCycleTime < maxInBuffer)
         maxCycleTime = maxInBuffer;
   }

   pos_cmd1 = Obj.CommandedPosition1;
   pos_cmd2 = Obj.CommandedPosition2;
   Obj.ActualPosition1 = Obj.CommandedPosition1;
   Obj.ActualPosition2 = Obj.CommandedPosition2;

   Step->stepgen_array[0].pos_scale = -Obj.StepsPerMM1;
   Step->stepgen_array[1].pos_scale = -Obj.StepsPerMM2;

   Obj.ActualPosition1 = Step->stepgen_array[0].pos_fb;
   Obj.ActualPosition2 = Step->stepgen_array[1].pos_fb;
   uint32_t diffT = longTime.extendTime(micros()) - irqTime;
   delayT = maxCycleTime + 50 - diffT - jitterThisCycle; // Add 50 as some saftey margin
   if (delayT > 0 && delayT < 900)
   {
      syncTimer->setOverflow(delayT, MICROSEC_FORMAT); // Work in flawed units, its ok
      syncTimer->refresh();
      syncTimer->resume();
   }
   else
   {
      syncWithLCNC();
   }
}
uint16_t oldCnt = 0;
uint64_t startTime = 0;
uint16_t avgTime = 0;
void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   // Obj.IndexStatus = Encoder1.indexHappened();
   // Obj.EncPos = Encoder1.currentPos();
   // Obj.EncFrequency = Encoder1.frequency(ESCvar.Time);
   // Obj.IndexByte = Encoder1.getIndexState();
   float_t ap2 = Obj.ActualPosition2;
#if 0
   uint64_t dTim = irqTime - thenTime; // Debug. Getting jitter over the last 200 milliseconds
   Tim.push(dTim);
   uint64_t max_Tim = 0, min_Tim = UINT64_MAX;
   for (decltype(Tim)::index_t i = 0; i < Tim.size(); i++)
   {
      uint32_t aTim = Tim[i];
      if (aTim > max_Tim)
         max_Tim = aTim;
      if (aTim < min_Tim)
         min_Tim = aTim;
   }
   thenTime = irqTime;
#endif
   if (irqCnt == 1000)
      startTime = irqTime;
   if (irqCnt == 11000)
      avgTime = (irqTime - startTime) / 1000;

   Obj.DiffT = longTime.extendTime(micros()) - irqTime; // max_Tim - min_Tim; // Debug
   uint16_t newCnt = isrTime;
   // Obj.D1 = newCnt - oldCnt;
   oldCnt = newCnt;
   Obj.D1 = 1000 * Obj.CommandedPosition2;        // abs(1000 * (ap2 - Obj.CommandedPosition2)); // Step2.actPos();
   Obj.D2 = 1000 * Step->stepgen_array[1].pos_fb; // Step->stepgen_array[1].rawcount % INT16_MAX;                          // Step->stepgen_array[1].freq;
   Obj.D3 = Step->stepgen_array[1].freq;
   Obj.D4 = Step->stepgen_array[1].rawcount % UINT16_MAX;
}

void ESC_interrupt_enable(uint32_t mask);
void ESC_interrupt_disable(uint32_t mask);
uint16_t dc_checker(void);
void sync0Handler(void);

static esc_cfg_t config =
    {
        .user_arg = NULL,
        .use_interrupt = 1,
        .watchdog_cnt = 150,
        .set_defaults_hook = NULL,
        .pre_state_change_hook = NULL,
        .post_state_change_hook = NULL,
        .application_hook = handleStepper,
        .safeoutput_override = NULL,
        .pre_object_download_hook = NULL,
        .post_object_download_hook = NULL,
        .rxpdo_override = NULL,
        .txpdo_override = NULL,
        .esc_hw_interrupt_enable = ESC_interrupt_enable,
        .esc_hw_interrupt_disable = ESC_interrupt_disable,
        .esc_hw_eep_handler = NULL,
        .esc_check_dc_handler = dc_checker,
};

void measureCrystalFrequency(void);

volatile byte serveIRQ = 0;

void setup(void)
{
   Serial1.begin(115200);
#if 0
   measureCrystalFrequency(); // Calibrate crystal frequency
#endif
   // rcc_config(); // Needed by encoder, probably breaks some timers.
   ecat_slv_init(&config);

   pinMode(PA11, OUTPUT); // Step X
   pinMode(PA12, OUTPUT); // Dir X
   pinMode(PC9, OUTPUT);  // Step Z
   pinMode(PC10, OUTPUT); // Dir Z

   Step = new StepGen3;

   baseTimer = new HardwareTimer(TIM1); // The base period timer
   uint32_t usecs = BASE_PERIOD / 1000;
   setFrequencyAdjustedMicrosSeconds(baseTimer, usecs);
   baseTimer->attachInterrupt(basePeriodCB);

   syncTimer = new HardwareTimer(TIM3); // The Linuxcnc servo period sync timer
   syncTimer->attachInterrupt(syncWithLCNC);
}

void loop(void)
{
   uint64_t dTime;
   if (serveIRQ)
   {
      nowTime = longTime.extendTime(micros());
      /* Read local time from ESC*/
      // ESC_read(ESCREG_LOCALTIME, (void *)&ESCvar.Time, sizeof(ESCvar.Time));
      // ESCvar.Time = etohl(ESCvar.Time);
      DIG_process(ALEventIRQ, DIG_PROCESS_WD_FLAG | DIG_PROCESS_OUTPUTS_FLAG |
                                  DIG_PROCESS_APP_HOOK_FLAG | DIG_PROCESS_INPUTS_FLAG);
      serveIRQ = 0;
      ESCvar.PrevTime = ESCvar.Time;
      ecat_slv_poll();
   }
   dTime = longTime.extendTime(micros()) - irqTime;
   if (dTime > 5000) // Don't run ecat_slv_poll when expecting to serve interrupt
      ecat_slv_poll();
}

void sync0Handler(void)
{
   irqTime = longTime.extendTime(micros());
   ALEventIRQ = ESC_ALeventread();
   serveIRQ = 1;
   irqCnt++;
}

// Enable SM2 interrupts
void ESC_interrupt_enable(uint32_t mask)
{
   // Enable interrupt for SYNC0 or SM2 or SM3
   // uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 | ESCREG_ALEVENT_SM3;
   uint32_t user_int_mask = ESCREG_ALEVENT_SM2; // Only SM2
   if (mask & user_int_mask)
   {
      ESC_ALeventmaskwrite(ESC_ALeventmaskread() | (mask & user_int_mask));
      ESC_ALeventmaskwrite(ESC_ALeventmaskread() & ~(ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM3));
      attachInterrupt(digitalPinToInterrupt(PC3), sync0Handler, RISING);

      // Set LAN9252 interrupt pin driver as push-pull active high
      uint32_t bits = 0x00000111;
      ESC_write(0x54, &bits, 4);

      // Enable LAN9252 interrupt
      bits = 0x00000001;
      ESC_write(0x5c, &bits, 4);
   }
}

// Disable SM2 interrupts
void ESC_interrupt_disable(uint32_t mask)
{
   // Enable interrupt for SYNC0 or SM2 or SM3
   // uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 | ESCREG_ALEVENT_SM3;
   uint32_t user_int_mask = ESCREG_ALEVENT_SM2;

   if (mask & user_int_mask)
   {
      // Disable interrupt from SYNC0
      ESC_ALeventmaskwrite(ESC_ALeventmaskread() & ~(mask & user_int_mask));
      detachInterrupt(digitalPinToInterrupt(PC3));
      // Disable LAN9252 interrupt
      uint32_t bits = 0x00000000;
      ESC_write(0x5c, &bits, 4);
   }
}

extern "C" uint32_t ESC_SYNC0cycletime(void);

// Setup of DC
uint16_t dc_checker(void)
{
   // Indicate we run DC
   ESCvar.dcsync = 1;
   // StepGen3::sync0CycleTime = ESC_SYNC0cycletime(); // nsecs
   return 0;
}

//
// Code to calibrate the crystal.
//

#include <HardwareTimer.h>
HardwareTimer *timer;
volatile uint32_t cnt;

void CB(void)
{
   if (cnt-- == 0)
   {
      timer->pause();
   }
}

void setFrequencyAdjustedMicrosSeconds(HardwareTimer *timer, uint32_t usecs)
{
   const uint16_t calibrated1000 = 1042;                                               // <- This is the factor to adjust to make 1 sec = 1 sec
   uint32_t period_cyc = (usecs * (timer->getTimerClkFreq() / 1000)) / calibrated1000; // Avoid overflow during math
   uint32_t Prescalerfactor = (period_cyc / 0x10000) + 1;
   uint32_t PeriodTicks = period_cyc / Prescalerfactor;
   timer->setPrescaleFactor(Prescalerfactor);
   timer->setOverflow(PeriodTicks, TICK_FORMAT);
   // Serial1.printf("Period_cyc=%u Prescalefactor =%u ticks = %u\n", period_cyc, Prescalerfactor, PeriodTicks);
}

void measureCrystalFrequency(void)
{
   timer = new HardwareTimer(TIM1);
   Serial1.begin(115200);
   delay(3000);
   Serial1.printf("Clock freq = %u\n", timer->getTimerClkFreq());
   setFrequencyAdjustedMicrosSeconds(timer, 1000);
   timer->refresh();
   timer->attachInterrupt(CB);
   cnt = 10000;

   Serial1.printf("\n");
   uint32_t startT = micros();
   timer->resume();

   while (cnt != 0)
      ;

   uint32_t endT = micros();
   uint32_t diffT = endT - startT;
   Serial1.printf("\n");
   Serial1.printf("diff = %u\n", diffT);

   Serial1.printf("\n");
   delay(10000);
   Serial1.printf("\n");

   exit(0);
}
