#include <Arduino.h>

#include <stdio.h>
extern "C"
{
#include "ecat_slv.h"
#include "utypes.h"
};
#include <CircularBuffer.h>
#define RINGBUFFERLEN 101
CircularBuffer<double_t, RINGBUFFERLEN> Pos;
CircularBuffer<uint32_t, RINGBUFFERLEN> TDelta;

int64_t PreviousEncoderCounterValue = 0;
int64_t unwrap_encoder(uint16_t in, int64_t *prev);
#include <Stm32F4_Encoder.h>
Encoder EncoderInit;
Encoder *encP = &EncoderInit;

// #include "Stepper.h"
#define INDEX_PIN PA2
HardwareSerial Serial1(PA10, PA9);
_Objects Obj;

void indexPulse(void);
double PosScaleRes = 1.0;
uint32_t CurPosScale = 1;
uint8_t OldLatchCEnable = 0;
volatile uint8_t indexPulseFired = 0;
uint32_t nFires = 0;
volatile uint8_t pleaseZeroTheCounter = 0;
uint32_t PrevTime = 0, Prev2Time = 0;

void cb_set_outputs(void) // Master outputs gets here, slave inputs, first operation
{
   if (Obj.IndexLatchEnable && !OldLatchCEnable) // Should only happen first time IndexCEnable is set
   {
      pleaseZeroTheCounter = 1;
   }
   OldLatchCEnable = Obj.IndexLatchEnable;

   if (CurPosScale != Obj.EncPosScale && Obj.EncPosScale != 0)
   {
      CurPosScale = Obj.EncPosScale;
      PosScaleRes = 1.0 / double(CurPosScale);
   }
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   Obj.IndexStatus = 0;
   if (indexPulseFired)
   {
      Obj.IndexStatus = 1;
      indexPulseFired = 0;
      nFires++;
      PreviousEncoderCounterValue = 0;
   }
   uint64_t now = micros();
   Obj.DiffT = now - Prev2Time;
   Prev2Time = PrevTime;
   PrevTime = now;

   int64_t pos = unwrap_encoder(TIM2->CNT, &PreviousEncoderCounterValue);
   double CurPos = pos * PosScaleRes;
   Obj.EncPos = CurPos;

   double diffT = 0;
   double diffPos = 0;
   TDelta.push(ESCvar.Time); // Running average over the length of the circular buffer
   Pos.push(CurPos);
   if (Pos.size() >= 2)
   {
      diffT = 1.0e-9 * (TDelta.last() - TDelta.first()); // Time is in nanoseconds
      diffPos = fabs(Pos.last() - Pos.first());
   }
   Obj.EncFrequency = diffT != 0 ? diffPos / diffT : 0.0; // Revolutions per second

   Obj.IndexStatus = 0;
   if (indexPulseFired)
   {
      Obj.IndexStatus = 1;
      indexPulseFired = 0;
      nFires++;
      PreviousEncoderCounterValue = 0;
   }
   Obj.IndexByte = digitalRead(INDEX_PIN);
   if (Obj.IndexByte)
      Serial1.printf("IS 1\n");
}

static esc_cfg_t config =
    {
        .user_arg = NULL,
        .use_interrupt = 0,
        .watchdog_cnt = 150,
        .set_defaults_hook = NULL,
        .pre_state_change_hook = NULL,
        .post_state_change_hook = NULL,
        .application_hook = NULL, // StepGen,
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
   Serial1.begin(115200);
   rcc_config();

   // Set starting count value
   EncoderInit.SetCount(Tim2, 0);
   // EncoderInit.SetCount(Tim3, 0);
   // EncoderInit.SetCount(Tim4, 0);
   // EncoderInit.SetCount(Tim8, 0);

   ecat_slv_init(&config);
   attachInterrupt(digitalPinToInterrupt(INDEX_PIN), indexPulse, RISING); // Always when Index triggered
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

void indexPulse(void)
{
   if (pleaseZeroTheCounter)
   {
      TIM2->CNT = 0;
      indexPulseFired = 1;
      Pos.clear();
      TDelta.clear();
      pleaseZeroTheCounter = 0;
   }
}