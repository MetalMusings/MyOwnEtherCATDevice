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
uint16_t sync0CycleTime;      // usecs

#include "MyEncoder.h"
volatile uint16_t encCnt = 0;
void indexPulseEncoderCB1(void);
MyEncoder Encoder1(TIM2, PA2, indexPulseEncoderCB1);
void indexPulseEncoderCB1(void)
{
   encCnt++;
   Encoder1.indexPulse();
}
// #include <RunningAverage.h>
// RunningAverage irqServeDelays(1000); // To get the max delay of the irq serve time over the last second
CircularBuffer<uint16_t, 1000> irqServeDelays;

#include "StepGen3.h"
StepGen3 *Step = 0;

#include "extend32to64.h"

volatile uint64_t irqTime = 0, irqCnt = 0;
extend32to64 longTime;

void setFrequencyAdjustedMicrosSeconds(HardwareTimer *timer, uint32_t usecs);

void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   Encoder1.setLatch(Obj.IndexLatchEnable);
   Encoder1.setScale(2000);
}

volatile uint16_t basePeriodCnt;
volatile uint64_t makePulsesCnt = 0, prevMakePulsesCnt = 0;
volatile uint16_t deltaMakePulsesCnt;

volatile double pos_cmd1, pos_cmd2;
void syncWithLCNC()
{
   syncTimer->pause();
   baseTimer->pause();
   deltaMakePulsesCnt = makePulsesCnt - prevMakePulsesCnt;
   prevMakePulsesCnt = makePulsesCnt;
   Step->updateStepGen(pos_cmd1, pos_cmd2);    // Update positions
   Step->makeAllPulses();                      // Make first step right here
   basePeriodCnt = SERVO_PERIOD / BASE_PERIOD; //
   baseTimer->refresh();                       //
   baseTimer->resume();                        // Make the other steps in ISR
}

void basePeriodCB(void)
{
   if (--basePeriodCnt > 0) // Stop
      Step->makeAllPulses();
   else
      baseTimer->pause();
}

int32_t delayT;
uint16_t thisCycleTime; // In usecs
int16_t maxIrqServeTime = 0;
uint64_t oldIrqTime = 0;
uint16_t nLoops;

void handleStepper(void)
{
   if (oldIrqTime != 0) // See if there is a delay in data, normally it *should* be nLoops=1, but sometimes it is more
   {
      thisCycleTime = irqTime - oldIrqTime;
      nLoops = round(float(thisCycleTime) / float(sync0CycleTime));
   }
   oldIrqTime = irqTime;

   uint32_t diffT = longTime.extendTime(micros()) - irqTime; // Time from interrupt was received by isr
   irqServeDelays.push(diffT);
   if (irqServeDelays.isFull()) // Do max calcs, just waiting a second
   {
      uint16_t maxInBuffer = 0;
      using index_t = decltype(irqServeDelays)::index_t;
      for (index_t i = 0; i < irqServeDelays.size(); i++)
      {
         if (maxInBuffer < irqServeDelays[i])
            maxInBuffer = irqServeDelays[i];
      }
      if (maxIrqServeTime > maxInBuffer) // Reduce by one, slowly eating up excess time
         maxIrqServeTime--;
      if (maxIrqServeTime < maxInBuffer)
         maxIrqServeTime = maxInBuffer;
   }

   pos_cmd1 = Obj.CommandedPosition1;
   pos_cmd2 = Obj.CommandedPosition2;

   Obj.ActualPosition1 = Obj.CommandedPosition1;
   Obj.ActualPosition2 = Obj.CommandedPosition2;

   Step->stepgen_array[0].pos_scale = -Obj.StepsPerMM1;
   Step->stepgen_array[1].pos_scale = -Obj.StepsPerMM2;

   // Obj.ActualPosition1 = Step->stepgen_array[0].pos_fb;
   // Obj.ActualPosition2 = Step->stepgen_array[1].pos_fb;

   delayT = maxIrqServeTime - diffT; // Add 10 as some safety margin
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

float_t oldCommandedPosition = 0;
void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   Obj.IndexStatus = Encoder1.indexHappened();
   Obj.EncPos = Encoder1.currentPos();
   Obj.EncFrequency = Encoder1.frequency(longTime.extendTime(micros()));
   Obj.IndexByte = Encoder1.getIndexState();

   Obj.DiffT = nLoops;
   Obj.D1 = 1000 * Obj.CommandedPosition2;                          // abs(1000 * (ap2 - Obj.CommandedPosition2)); // Step2.actPos();
   Obj.D2 = 1000 * Obj.ActualPosition2;                             // Step->stepgen_array[1].pos_fb; // Step->stepgen_array[1].rawcount % INT16_MAX;                          // Step->stepgen_array[1].freq;
   Obj.D3 = encCnt % 256;                                           // Step->stepgen_array[1].freq;
   Obj.D4 = 1000 * (Obj.CommandedPosition2 - oldCommandedPosition); // deltaMakePulsesCnt;            // Step->stepgen_array[1].rawcount % UINT16_MAX;
   oldCommandedPosition = Obj.CommandedPosition2;
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
   Step = new StepGen3;

   encoder_config(); // Needed by encoder, probably breaks some timers.
   ecat_slv_init(&config);

   pinMode(PA11, OUTPUT); // Step X
   pinMode(PA12, OUTPUT); // Dir X
   pinMode(PC9, OUTPUT);  // Step Z
   pinMode(PC10, OUTPUT); // Dir Z

   baseTimer = new HardwareTimer(TIM11); // The base period timer
   setFrequencyAdjustedMicrosSeconds(baseTimer, BASE_PERIOD / 1000);
   // baseTimer->setOverflow(BASE_PERIOD / 1000, MICROSEC_FORMAT); // Or the line above, This one is uncalibrated
   baseTimer->attachInterrupt(basePeriodCB);

   syncTimer = new HardwareTimer(TIM3); // The Linuxcnc servo period sync timer
   syncTimer->attachInterrupt(syncWithLCNC);
}

void loop(void)
{
   uint64_t dTime;
   if (serveIRQ)
   {
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
   ALEventIRQ = ESC_ALeventread();
   // if (ALEventIRQ & ESCREG_ALEVENT_SM2)
   {
      irqTime = longTime.extendTime(micros());
      serveIRQ = 1;
   }
   irqCnt++; // debug output
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
   sync0CycleTime = ESC_SYNC0cycletime() / 1000; // usecs
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
