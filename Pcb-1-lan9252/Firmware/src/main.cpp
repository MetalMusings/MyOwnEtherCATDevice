#include <Arduino.h>

#include <stdio.h>
extern "C"
{
#include "ecat_slv.h"
#include "utypes.h"
};

int64_t PreviousEncoderCounterValue = 0;
int64_t unwrap_encoder(uint16_t in, int64_t *prev);
#include <Stm32F4_Encoder.h>
Encoder EncoderInit;

#include "Stepper.h"

HardwareSerial Serial1(PA10, PA9);
_Objects Obj;

void StepGen(void);

volatile uint8_t IndexEnable = 1;
volatile uint8_t IndexTriggered = 0;
void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   IndexEnable = Obj.EncoderIn.IndexEnable;
   if (!IndexEnable)
      IndexTriggered = 0;
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   // Obj.EncoderOut.ECount = TIM2->CNT;
   Obj.EncoderOut.ECount = unwrap_encoder(TIM2->CNT, &PreviousEncoderCounterValue);
   Obj.EncoderOut.IndexTriggered = IndexTriggered;
   uint32_t diffT = ESCvar.Time - ESCvar.PrevTime;
}

void indexPulse(void)
{
   if (IndexEnable && !IndexTriggered)
   {
      TIM2->CNT = 0;
      IndexTriggered = 1;
   }
}
static esc_cfg_t config =
    {
        .user_arg = NULL,
        .use_interrupt = 0,
        .watchdog_cnt = 150,
        .set_defaults_hook = NULL,
        .pre_state_change_hook = NULL,
        .post_state_change_hook = NULL,
        .application_hook = StepGen,
        .safeoutput_override = NULL,
        .pre_object_download_hook = NULL,
        .post_object_download_hook = NULL,
        .rxpdo_override = NULL,
        .txpdo_override = NULL,
        .esc_hw_interrupt_enable = NULL,
        .esc_hw_interrupt_disable = NULL,
        .esc_hw_eep_handler = NULL,
        .esc_check_dc_handler = NULL,
};

void setup(void)
{
   uint64_t ff = F_CPU;
   Serial1.begin(115200);
   rcc_config();

   // Set starting count value
   // EncoderInit.SetCount(Tim2, 0);
   // EncoderInit.SetCount(Tim3, 0);
   // EncoderInit.SetCount(Tim4, 0);
   // EncoderInit.SetCount(Tim8, 0);

   attachInterrupt(digitalPinToInterrupt(PA2), indexPulse, RISING); // PA2 = Index pulse
   StepperSetup();
// delay(5000); // To give serial port monitor time to receive
   Serial1.printf("Before Ecat config\n");
   ecat_slv_init(&config);

   Serial1.printf("Started\n");
}

void loop(void)
{
   ESCvar.PrevTime = ESCvar.Time;
   ecat_slv();
}

#define ONE_PERIOD 65536
#define HALF_PERIOD 32768

int64_t unwrap_encoder(uint16_t in, int64_t *prev)
{
   int64_t c64 = (int32_t)in - HALF_PERIOD; // remove half period to determine (+/-) sign of the wrap
   int64_t dif = (c64 - *prev);             // core concept: prev + (current - prev) = current

   // wrap difference from -HALF_PERIOD to HALF_PERIOD. modulo prevents differences after the wrap from having an incorrect result
   int64_t mod_dif = ((dif + HALF_PERIOD) % ONE_PERIOD) - HALF_PERIOD;
   if (dif < -HALF_PERIOD)
      mod_dif += ONE_PERIOD; // account for mod of negative number behavior in C

   int64_t unwrapped = *prev + mod_dif;
   *prev = unwrapped; // load previous value

   return unwrapped + HALF_PERIOD; // remove the shift we applied at the beginning, and return
}

void StepGen(void)
{
   uint16_t Period = Obj.StepperData.Period; // Period in microseconds, so 1000 is 1 ms.

   float StepperResolution = Obj.StepperData.Resolution;   // 2.5 pulses/um
   int32_t TargetPosition = Obj.TargetPosition;            // um
   int32_t ActualPosition = 0;                             // um
   int32_t DistanceToGo = TargetPosition - ActualPosition; // um
   int32_t PulsesToMake = DistanceToGo * StepperResolution;
   int32_t Frequency = PulsesToMake * 1000000 / Period;

   // Check if timer done.
}