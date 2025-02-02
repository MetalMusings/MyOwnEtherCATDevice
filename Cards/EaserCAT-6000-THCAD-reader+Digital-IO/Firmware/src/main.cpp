#include <Arduino.h>
#include <stdio.h>
extern "C"
{
#include "ecat_slv.h"
#include "utypes.h"
};
_Objects Obj;

#include "extend32to64.h"
extend32to64 longTime;
volatile uint64_t irqTime = 0;

HardwareSerial Serial1(PA10, PA9);

uint8_t inputPin[] = {PD15, PD14, PD13, PD12, PD11, PD10, PD9, PD8, PB15, PB14, PB13, PB12};
uint8_t outputPin[] = {PE10, PE9, PE8, PE7};

#include "HardwareTimer.h"
// NOTE 1. Optional optional, not needed. This mod in the beginning of HardwareTimer.cpp for 32-bit precision
////// //#define MAX_RELOAD ((1 << 16) - 1) // Currently even 32b timers are used as 16b to have generic behavior
////// #define MAX_RELOAD 0xFFFFFFFF
// Note 2. This is optional. A 16-bit timer is enough to capture the THCAD frequency.
// Lowest freq on my THCAD card is 3.8 kHz at 0 V and then faster for higher voltage
// Lowest freq possible with 16-bit timer is 168000000/65535 = 2.5 kHz.

#define THCAD_PIN PA0
// PA0 is connected to Timer 2, a 32-bit timer

uint32_t channel;
volatile uint32_t FrequencyMeasured, LastCapture = 0, CurrentCapture;
uint32_t input_freq = 0;
volatile uint32_t rolloverCompareCount = 0;
HardwareTimer *EncoderTimer;
void InputCapture_IT_callback(void);
void Rollover_IT_callback(void);

#define bitset(byte, nbit) ((byte) |= (1 << (nbit)))
#define bitclear(byte, nbit) ((byte) &= ~(1 << (nbit)))
#define bitflip(byte, nbit) ((byte) ^= (1 << (nbit)))
#define bitcheck(byte, nbit) ((byte) & (1 << (nbit)))

volatile uint16_t ALEventIRQ; // ALEvent that caused the interrupt

void cb_set_outputs(void) // Get Master outputs, slave inputs, first operation
{
   // Update digital output pins
   for (int i = 0; i < sizeof(outputPin); i++)
      digitalWrite(outputPin[i], bitcheck(Obj.Output4, i) ? HIGH : LOW);
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   for (int i = 0; i < sizeof(inputPin); i++)
      Obj.Input12 = digitalRead(inputPin[i]) == HIGH ? bitset(Obj.Input12, i) : bitclear(Obj.Input12, i);
   float scale = Obj.VelocityScale;
   if (scale == 0.0)
      scale = 1.0;
   Obj.Velocity = scale * FrequencyMeasured;
   Obj.Frequency = FrequencyMeasured;
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
        .application_hook = NULL,
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

   for (int i = 0; i < sizeof(inputPin); i++)
      pinMode(inputPin[i], INPUT_PULLDOWN);
   for (int i = 0; i < sizeof(outputPin); i++)
   {
      pinMode(outputPin[i], OUTPUT);
      digitalWrite(outputPin[i], LOW);
   }
#if 0
   // Debug leds
   pinMode(PB4, OUTPUT);
   pinMode(PB5, OUTPUT);
   pinMode(PB6, OUTPUT);
   pinMode(PB7, OUTPUT);
   digitalWrite(PB4, HIGH);
   digitalWrite(PB5, HIGH);
   digitalWrite(PB6, HIGH);
   digitalWrite(PB7, HIGH);
#endif

   // Automatically retrieve TIM instance and channel associated to pin
   // This is used to be compatible with all STM32 series automatically.
   TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(THCAD_PIN), PinMap_PWM);
   channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(THCAD_PIN), PinMap_PWM));

   EncoderTimer = new HardwareTimer(Instance);

   // Configure rising edge detection to measure frequency
   EncoderTimer->setMode(channel, TIMER_INPUT_CAPTURE_RISING, THCAD_PIN);

   // With a PrescalerFactor = 1, the minimum frequency value to measure is : TIM counter clock / CCR MAX
   //  = (SystemCoreClock) / 65535
   // Example on Nucleo_L476RG with systemClock at 80MHz, the minimum frequency is around 1,2 khz
   // To reduce minimum frequency, it is possible to increase prescaler. But this is at a cost of precision.
   // The maximum frequency depends on processing of the interruption and thus depend on board used
   // Example on Nucleo_L476RG with systemClock at 80MHz the interruption processing is around 4,5 microseconds and thus Max frequency is around 220kHz

   // A 16 bit timer is ok. Can measure down to 2.6 kHz, which is lower than 0 Volt on my card (3.8 kHz)
   // But I have 32-bit timer for historical reasons
   uint32_t PrescalerFactor = 1;
   EncoderTimer->setPrescaleFactor(PrescalerFactor);
   EncoderTimer->setOverflow(0xFFFFFFF0); // Max Period value to have the largest possible time to detect rising edge and avoid timer rollover
   EncoderTimer->attachInterrupt(channel, InputCapture_IT_callback);
   EncoderTimer->attachInterrupt(Rollover_IT_callback);
   EncoderTimer->resume();

   // Compute this scale factor only once
   input_freq = EncoderTimer->getTimerClkFreq() / EncoderTimer->getPrescaleFactor();

   ecat_slv_init(&config);
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
   if (dTime > 5000) // Not doing interrupts - handle free-run
      ecat_slv();
}

void sync0Handler(void)
{
   ALEventIRQ = ESC_ALeventread();
   // if (ALEventIRQ & ESCREG_ALEVENT_SM2)
   {
      irqTime = longTime.extendTime(micros());
      serveIRQ = 1;
   }
}

// Enable SM2 interrupts
void ESC_interrupt_enable(uint32_t mask)
{
   // Enable interrupt for SYNC0 or SM2 or SM3
   uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 | ESCREG_ALEVENT_SM3;
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
   return 0;
}

void InputCapture_IT_callback(void)
{
   CurrentCapture = EncoderTimer->getCaptureCompare(channel);

   /* frequency computation */
   if (CurrentCapture > LastCapture)
   {
      FrequencyMeasured = input_freq / (CurrentCapture - LastCapture);
   }
   else if (CurrentCapture <= LastCapture)
   {
      /* 0xFFFFFFFF is max overflow value */
      FrequencyMeasured = input_freq / (0xFFFFFFFF + CurrentCapture - LastCapture);
   }
   LastCapture = CurrentCapture;
   rolloverCompareCount = 0;
}

/* In case of timer rollover, frequency is to low to be measured set value to 0
   To reduce minimum frequency, it is possible to increase prescaler. But this is at a cost of precision. */
void Rollover_IT_callback(void)
{
   rolloverCompareCount++;

   if (rolloverCompareCount > 1)
   {
      FrequencyMeasured = 0;
   }
}
