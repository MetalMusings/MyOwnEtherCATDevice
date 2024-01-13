#include <Arduino.h>
#include <stdio.h>
extern "C"
{
#include "ecat_slv.h"
#include "utypes.h"
};
_Objects Obj;

HardwareSerial Serial1(PA10, PA9);

#include "MyEncoder.h"
void indexPulseEncoderCB1(void);
MyEncoder Encoder1(TIM2, PA2, indexPulseEncoderCB1);
void indexPulseEncoderCB1(void)
{
   Encoder1.indexPulse();
}
#if 0
void indexPulseEncoderCB2(void);
MyEncoder Encoder2(TIM3, PB6, indexPulseEncoderCB2);
void indexPulseEncoderCB2(void)
{
   Encoder2.indexPulse();
}
#endif

#include "StepGen.h"
void timerCallbackStep1(void);
StepGen Step1(TIM1, 4, PA11, PA12, timerCallbackStep1);
void timerCallbackStep1(void)
{
   Step1.timerCB();
}
void timerCallbackStep2(void);
StepGen Step2(TIM8, 4, PC9, PC10, timerCallbackStep2);
void timerCallbackStep2(void)
{
   Step2.timerCB();
}

void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   Encoder1.setLatch(Obj.IndexLatchEnable);
   Encoder1.setScale(Obj.EncPosScale);

   Step1.cmdPos(Obj.StepGenIn1.CommandedPosition);
   Step2.cmdPos(Obj.StepGenIn2.CommandedPosition);
}

void handleStepper(void)
{
   Step1.handleStepper();
   Step2.handleStepper();
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   Obj.IndexStatus = Encoder1.indexHappened();
   Obj.EncPos = Encoder1.currentPos();
   Obj.EncFrequency = Encoder1.frequency(ESCvar.Time);
   Obj.IndexByte = Encoder1.getIndexState();

   Obj.StepGenOut1.ActualPosition = Step1.actPos();
   Obj.StepGenOut2.ActualPosition = Step2.actPos();
   Obj.DiffT = 10000 * Step1.reqPos(); // Debug
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
   rcc_config();

   Step1.setScale(1250); // 2000 /rev 4 mm/rev x 2.5 gear => 1250 /mm
   Step2.setScale(500);  // 2000 /rev 4 mm/rev => 500 /mm

   ecat_slv_init(&config);
}

void loop(void)
{
   ESCvar.PrevTime = ESCvar.Time;
   if (serveIRQ)
   {
      DIG_process(DIG_PROCESS_WD_FLAG | DIG_PROCESS_OUTPUTS_FLAG |
                  DIG_PROCESS_APP_HOOK_FLAG | DIG_PROCESS_INPUTS_FLAG);
      serveIRQ = 0;
   }
   ecat_slv_poll();
}

void sync0Handler(void)
{
   serveIRQ = 1;
}

void ESC_interrupt_enable(uint32_t mask)
{
   // Enable interrupt for SYNC0 or SM2 or SM3
   uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 |
                            ESCREG_ALEVENT_SM2 |
                            ESCREG_ALEVENT_SM3;
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
   uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 |
                            ESCREG_ALEVENT_SM2 |
                            ESCREG_ALEVENT_SM3;

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
   ESCvar.dcsync = 0;
   StepGen::sync0CycleTime = ESC_SYNC0cycletime() / 1000;
   return 0;
}
