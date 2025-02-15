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

#include "MyMCP3221.h"
MyMCP3221 mcp3221_0(0x48, &Wire2);
MyMCP3221 mcp3221_7(0x4f, &Wire2);

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
   static float validData0 = 0.0, validVoltage0 = 0.0;
   for (int i = 0; i < sizeof(inputPin); i++)
      Obj.Input12 = digitalRead(inputPin[i]) == HIGH ? bitset(Obj.Input12, i) : bitclear(Obj.Input12, i);

   float scale = Obj.VoltageScale;
   if (scale == 0.0)
      scale = 1.0;
   int data0 = mcp3221_0.getData();
   if ((Obj.Status = mcp3221_0.ping()) == 0)
   {                                                      // Read good value
      Obj.CalculatedVoltage = scale * data0 + Obj.VoltageOffset; //
      Obj.RawData = data0;                                // Raw voltage, read by ADC
      validVoltage0 = Obj.CalculatedVoltage;
      validData0 = data0;
   }
   else
   {                                         // Didn't read a good value. Return a hopefully useful value and restart the I2C bus
      Obj.CalculatedVoltage = validVoltage0; // Use value from previous call
      Obj.RawData = validData0;
      // Reset wire here
   }
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
#if 1
   // Debug leds
   pinMode(PB4, OUTPUT);
   pinMode(PB5, OUTPUT);
   pinMode(PB6, OUTPUT);
   pinMode(PB7, OUTPUT);
   digitalWrite(PB4, HIGH);
   digitalWrite(PB5, HIGH);
   digitalWrite(PB6, HIGH);
   digitalWrite(PB7, HIGH);
   digitalWrite(PB4, LOW);
   digitalWrite(PB5, LOW);
   digitalWrite(PB6, LOW);
   digitalWrite(PB7, LOW);
   digitalWrite(outputPin[0], HIGH);
   digitalWrite(outputPin[1], HIGH);
   digitalWrite(outputPin[2], HIGH);
   digitalWrite(outputPin[3], HIGH);
#endif

   Wire2.begin();
   Wire2.setClock(400000);

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

   Serial1.println("\nI2C Scanner");
   Serial1.printf("Ping0=%d\n", mcp3221_0.ping());
   Serial1.printf("Ping7=%d\n", mcp3221_7.ping());
   int16_t res0 = mcp3221_0.getData();
   int16_t v0 = (res0 * 5027 * 39) / 4096;
   Serial1.printf("Voltage_0: %d Voltage_7: %d\n", v0, res0);
   delay(1000);
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
