#include <Arduino.h>
#include <stdio.h>
extern "C"
{
#include "ecat_slv.h"
#include "utypes.h"
};
_Objects Obj;

HardwareSerial Serial1(PA10, PA9);

#define DEBUG_TIM8 1
#include "MyEncoder.h"
void indexPulseEncoderCB1(void);
MyEncoder Encoder1(TIM2, PA2, indexPulseEncoderCB1);
void indexPulseEncoderCB1(void)
{
   Encoder1.indexPulse();
}
#if 0
#include "StepGen.h"
void timerCallbackStep1(void);
StepGen Step1(TIM1, 4, PA_11, PA12, timerCallbackStep1);
void timerCallbackStep1(void)
{
   Step1.timerCB();
}
void timerCallbackStep2(void);
StepGen Step2(TIM3, 4, PC_9, PC10, timerCallbackStep2);
void timerCallbackStep2(void)
{
   Step2.timerCB();
}
#endif
#include "StepGen2.h"
void pulseTimerCallback(void);
void startTimerCallback(void);
StepGen2 Step(TIM1, 4, PA_11, PA12, pulseTimerCallback, TIM10, startTimerCallback);
void pulseTimerCallback(void) { Step.pulseTimerCB(); }
void startTimerCallback(void) { Step.startTimerCB(); }
CircularBuffer<uint32_t, 200> Tim;
volatile uint64_t nowTime = 0, thenTime = 0;

void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   Encoder1.setLatch(Obj.IndexLatchEnable);
   Encoder1.setScale(Obj.EncPosScale);
#if 0
   Step1.reqPos(Obj.StepGenIn1.CommandedPosition);
   Step1.setScale(Obj.StepGenIn1.StepsPerMM);
   Step1.enable(Obj.Enable1);
   Step2.reqPos(Obj.StepGenIn2.CommandedPosition);
   Step2.setScale(Obj.StepGenIn2.StepsPerMM);
   Step2.enable(Obj.Enable1);
#endif
}

void handleStepper(void)
{
   Step.enabled = true;
   Step.commandedPosition = Obj.StepGenIn1.CommandedPosition;

#if 0
   Step1.handleStepper();
   Step2.handleStepper();
#endif
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   Obj.IndexStatus = Encoder1.indexHappened();
   Obj.EncPos = Encoder1.currentPos();
   Obj.EncFrequency = Encoder1.frequency(ESCvar.Time);
   Obj.IndexByte = Encoder1.getIndexState();
#if 0
   Obj.StepGenOut1.ActualPosition = Step1.actPos();
   Obj.StepGenOut2.ActualPosition = Step2.actPos();
#endif
   uint32_t dTim = nowTime - thenTime; // Debug. Getting jitter over the last 200 milliseconds
   Tim.push(dTim);
   uint32_t max_Tim = 0, min_Tim = UINT32_MAX;
   for (decltype(Tim)::index_t i = 0; i < Tim.size(); i++)
   {
      uint32_t aTim = Tim[i];
      if (aTim > max_Tim)
         max_Tim = aTim;
      if (aTim < min_Tim)
         min_Tim = aTim;
   }
   thenTime = nowTime;
   Obj.DiffT = max_Tim - min_Tim; // Debug
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
   if (serveIRQ)
   {
      nowTime = micros();
      DIG_process(DIG_PROCESS_WD_FLAG | DIG_PROCESS_OUTPUTS_FLAG |
                  DIG_PROCESS_APP_HOOK_FLAG | DIG_PROCESS_INPUTS_FLAG);
      serveIRQ = 0;
      ESCvar.PrevTime = ESCvar.Time;
   }
   uint32_t dTime = micros() - nowTime;
   if ((dTime > 200 && dTime < 500) || dTime > 1500) // Don't run ecat_slv_poll when expecting to serve interrupt
      ecat_slv_poll();
}

void sync0Handler(void)
{
   serveIRQ = 1;
}

void ESC_interrupt_enable(uint32_t mask)
{
   // Enable interrupt for SYNC0 or SM2 or SM3
   // uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 | ESCREG_ALEVENT_SM3;
   uint32_t user_int_mask = ESCREG_ALEVENT_SM2; // Only SM2
   if (mask & user_int_mask)
   {
      ESC_ALeventmaskwrite(ESC_ALeventmaskread() | (mask & user_int_mask));
      attachInterrupt(digitalPinToInterrupt(PC3), sync0Handler, RISING);

      // Set LAN9252 interrupt pin driver as push-pull active high
      uint32_t bits = 0x00000111;
      ESC_write(0x54, &bits, 4);

      // Enable LAN9252 interrupt
      bits = 0x00000001;
      ESC_write(0x5c, &bits, 4);
   }
}

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
#if 0
   StepGen::sync0CycleTime = ESC_SYNC0cycletime() / 1000;  // usecs
#endif
   StepGen2::sync0CycleTime = ESC_SYNC0cycletime() / 1000; // usecs
   return 0;
}
