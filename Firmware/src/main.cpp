#include <Arduino.h>
#include <HardwareTimer.h>

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

#include <Stm32F4_Encoder.h>
int64_t PreviousEncoderCounterValue = 0;
int64_t unwrap_encoder(uint16_t in, int64_t *prev);
Encoder EncoderInit;
Encoder *encP = &EncoderInit;

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

uint32_t sync0CycleTime = 0;

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
   // Obj.DiffT = sync0CycleTime;

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

   Obj.IndexByte = digitalRead(INDEX_PIN);
   if (Obj.IndexByte)
      Serial1.printf("IS 1\n");
}

void ESC_interrupt_enable(uint32_t mask);
void ESC_interrupt_disable(uint32_t mask);
uint16_t dc_checker(void);

static esc_cfg_t config =
    {
        .user_arg = NULL,
        .use_interrupt = 1,
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
        .esc_hw_interrupt_enable = ESC_interrupt_enable,
        .esc_hw_interrupt_disable = ESC_interrupt_disable,
        .esc_hw_eep_handler = NULL,
        .esc_check_dc_handler = dc_checker,
};
#define STEPPER_DIR_PIN PA12
#define STEPPER_STEP_PIN PA11
HardwareTimer *MyTim;
volatile uint32_t stepCount = 0, stepPulses = 0;
volatile double_t actualPosition = 0;
volatile double_t requestedPosition;
volatile uint32_t pulsesToGo = 0;
volatile byte forwardDirection = 0; // 1 if going forward

void TimerStep_CB(void)
{
   stepCount++;
   if (stepCount == stepPulses)
   {
      MyTim->pause();
   }
}

void makePulses(uint32_t period /* in usecs */, uint32_t pulses /* nr of pulses to do*/)
{
   MyTim->setOverflow(period / pulses, MICROSEC_FORMAT);
   MyTim->setCaptureCompare(4, 50, PERCENT_COMPARE_FORMAT); // 50 %
   stepCount = 0;
   stepPulses = pulses;
   MyTim->resume();
}

void sync0Handler(void);
void setup(void)
{
   Serial1.begin(115200);
   rcc_config();

   TIM_TypeDef *Instance = TIM1;
   MyTim = new HardwareTimer(Instance);
   MyTim->setMode(4, TIMER_OUTPUT_COMPARE_PWM2, STEPPER_STEP_PIN);
   MyTim->setCaptureCompare(4, 50, PERCENT_COMPARE_FORMAT); // 50 %
   MyTim->attachInterrupt(TimerStep_CB);
   pinMode(STEPPER_DIR_PIN, OUTPUT);

   uint32_t i = 2;
   while (1)
   {
      Obj.StepGenIn1.CommandedPosition = i;
      sync0Handler();
      HAL_Delay(1000);
      i++;
   }

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

void sync0Handler(void)
{
   const int32_t stepsPerMM = 1;
   // Update the actual position
   pulsesToGo = stepsPerMM * Obj.StepGenIn1.CommandedPosition; // Wrong, but test
   Obj.StepGenOut1.ActualPosition = actualPosition;
   // Get new end position
   forwardDirection = pulsesToGo > 1 ? 1 : 0;
   // Set direction pin
   Obj.DiffT = forwardDirection;
   digitalWrite(STEPPER_DIR_PIN, forwardDirection); // I think one should really wait a bit when changed

   // Make the pulses using hardware timer
   if (pulsesToGo != 0)
      makePulses(1000 /*sync0CycleTime / 1000*/, abs((int)pulsesToGo));
   actualPosition = Obj.StepGenIn1.CommandedPosition;
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
   ESCvar.dcsync = 1;
   sync0CycleTime = ESC_SYNC0cycletime();
   return 0;
}
