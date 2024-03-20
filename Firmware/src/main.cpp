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
#define DEBUG_TIM8 1
#include "MyEncoder.h"
void indexPulseEncoderCB1(void);
MyEncoder Encoder1(TIM2, PA2, indexPulseEncoderCB1);
void indexPulseEncoderCB1(void)
{
   Encoder1.indexPulse();
}

#include "StepGen2.h"
// Stepper 1
void pulseTimerCallback1(void);
void startTimerCallback1(void);
StepGen2 Step1(TIM1, 4, PA_11, PA12, pulseTimerCallback1, TIM10, startTimerCallback1);
void pulseTimerCallback1(void) { Step1.pulseTimerCB(); }
void startTimerCallback1(void) { Step1.startTimerCB(); }

// Stepper 2
void pulseTimerCallback2(void);
void startTimerCallback2(void);
StepGen2 Step2(TIM3, 4, PC_9, PC10, pulseTimerCallback2, TIM11, startTimerCallback2);
void pulseTimerCallback2(void) { Step2.pulseTimerCB(); }
void startTimerCallback2(void) { Step2.startTimerCB(); }

CircularBuffer<uint64_t, 200> Tim;
volatile uint64_t irqTime = 0, thenTime = 0, nowTime = 0;
extend32to64 longTime;

void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   Encoder1.setLatch(Obj.IndexLatchEnable);
   Encoder1.setScale(2000);

   // Step2.reqPos(Obj.CommandedPosition2);
   // Step2.setScale(Obj.StepsPerMM2);
   // Step2.enable(1);
}

uint16_t nLoops;
uint64_t reallyNowTime = 0, reallyThenTime = 0;
uint64_t timeDiff; // Timediff in nanoseconds
void handleStepper(void)
{
   // Catch the case when we miss a loop for some reason
   uint32_t t = micros();
   reallyNowTime = longTime.extendTime(t);
   timeDiff = 1000 * (reallyNowTime - reallyThenTime);
   nLoops = round(double(timeDiff) / double(StepGen2::sync0CycleTime));
   reallyThenTime = reallyNowTime;
nLoops=1;
   Step1.enabled = true;
   Step1.commandedPosition = Obj.CommandedPosition1;
   Step1.stepsPerMM = Obj.StepsPerMM1;
   Step1.handleStepper(irqTime, nLoops);

   Step2.enabled = true;
   Step2.commandedPosition = Obj.CommandedPosition2;
   Step2.stepsPerMM = Obj.StepsPerMM2;
   Step2.handleStepper(irqTime, nLoops);
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   Obj.IndexStatus = Encoder1.indexHappened();
   Obj.EncPos = Encoder1.currentPos();
   Obj.EncFrequency = Encoder1.frequency(ESCvar.Time);
   Obj.IndexByte = Encoder1.getIndexState();
   float_t ap2 = Obj.ActualPosition2;
   Obj.ActualPosition1 = Obj.CommandedPosition1; // Step1.actPos();
   Obj.ActualPosition2 = Obj.CommandedPosition2; // Step2.actPos();

   uint64_t dTim = nowTime - thenTime; // Debug. Getting jitter over the last 200 milliseconds
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
   Obj.DiffT = longTime.extendTime(micros()) - irqTime; // max_Tim - min_Tim; // Debug
   Obj.D1 = Step2.frequency;
   Obj.D2 = Step2.nSteps;
   Obj.D3 = abs(1000 * (ap2 - Obj.CommandedPosition2)); // Step2.actPos();
   Obj.D4 = Step2.Tstartu;
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

volatile byte serveIRQ = 0;

void setup(void)
{
   Serial1.begin(115200);
   rcc_config(); // probably breaks some timers.
   ecat_slv_init(&config);
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
   ALEventIRQ = ESC_ALeventread();
   // if (ALEventIRQ & ESCREG_ALEVENT_SM2)
   {
      serveIRQ = 1;
      irqTime = longTime.extendTime(micros());
   }
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
   StepGen2::sync0CycleTime = ESC_SYNC0cycletime(); // nsecs
   return 0;
}
