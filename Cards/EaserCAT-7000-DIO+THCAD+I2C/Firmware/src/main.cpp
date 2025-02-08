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

#include "Wire.h"
TwoWire Wire2(PB11, PB10);

#include "ADS1X15.h"
ADS1115 ADS(0x48, &Wire2);

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

   float scale = Obj.VoltageScale;
   if (scale == 0.0)
      scale = 1.0;
   float ADCvoltage = ADS.toVoltage(ADS.getValue());
   Obj.ArcVoltage = scale*ADCvoltage;   // * ADCvoltage; // Scaled voltage, to give Plasma arc voltage
   Obj.Voltage = ADCvoltage; // Raw voltage, read by ADC
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
        .esc_hw_interrupt_enable = NULL,  // ESC_interrupt_enable,
        .esc_hw_interrupt_disable = NULL, // ESC_interrupt_disable,
        .esc_hw_eep_handler = NULL,
        .esc_check_dc_handler = NULL, // dc_checker,
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

   Wire2.begin();

   ADS.begin();
   ADS.setGain(0);             //  0 = 6.144 volt, 1 = 4.096 V
   ADS.setDataRate(7);         //  0 = slow   4 = medium   7 = fast
   ADS.setMode(0);             //  continuous mode
   ADS.setWireClock(400000UL); // 400 kHz
   ADS.readADC(0);             //  first read to trigger settings

#ifdef ECAT
   ecat_slv_init(&config);
#endif
}

void loop(void)
{
#ifdef ECAT
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
#else
   Serial1.print("TIC ");
   int tot = 0;
   uint32_t before = millis();
   for (int i = 0; i < 10000; i++)
   {
      int16_t raw = ADS.getValue();
      tot += raw;
   }
   uint32_t after = millis();

   int16_t value = ADS.getValue();
   float fval = ADS.toVoltage(value);
   Serial1.printf("Time för 10000=%d värdet=", after - before);
   Serial1.println(fval);
#endif
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
