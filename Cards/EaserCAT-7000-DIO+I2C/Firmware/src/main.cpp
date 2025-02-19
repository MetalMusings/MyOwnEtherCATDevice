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

const uint32_t I2C_BUS_SPEED = 400000;
uint32_t I2C_restarts = 0;

#include "Wire.h"
TwoWire Wire2(PB11, PB10);

#ifdef MCP3221
#include "MyMCP3221.h"
MyMCP3221 mcp3221_0(0x48, &Wire2);
MyMCP3221 mcp3221_7(0x4f, &Wire2);
#endif
#ifdef ADS1xxx
#include "ADS1X15.h"
ADS1115 ads1014(0x48, &Wire2);
void ads1014_reset()
{
   ads1014.reset();
   ads1014.begin();
   ads1014.setGain(1);                 // 1=4.096V
   ads1014.setMode(0);                 // 0 continuous
   ads1014.setDataRate(6);             // Max for ads101x
   ads1014.readADC_Differential_0_1(); // This is the value we are interested in
}
#endif

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
#ifdef MCP3221
   int data0 = mcp3221_0.getData();
   int stat = mcp3221_0.ping();
#endif
#ifdef ADS1xxx
   int data0 = ads1014.getValue();
   int stat = ads1014.isConnected();
#endif
   if (stat == 0)
   {                                                             // Read good value
      Obj.CalculatedVoltage = scale * data0 + Obj.VoltageOffset; //
      Obj.RawData = data0;                                       // Raw voltage, read by ADC
      validVoltage0 = Obj.CalculatedVoltage;
      validData0 = data0;
   }
   else
   {                                         // Didn't read a good value. Return a hopefully useful value and restart the I2C bus
      Obj.CalculatedVoltage = validVoltage0; // Use value from previous call
      Obj.RawData = validData0;
      // Reset wire here
      Wire2.end();
      Wire2.begin();
      Wire2.setClock(I2C_BUS_SPEED);
      I2C_restarts++;
#ifdef ADS1xxx
      ads1014_reset();
#endif
   }
   Obj.Status = I2C_restarts + (stat << 28); // Put status as bits 28-31, the lower are number of restarts (restart attempts)
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
   // Debug leds
   pinMode(PB4, OUTPUT);
   pinMode(PB5, OUTPUT);
   pinMode(PB6, OUTPUT);
   pinMode(PB7, OUTPUT);
   digitalWrite(PB4, LOW);
   digitalWrite(PB5, LOW);
   digitalWrite(PB6, LOW);
   digitalWrite(PB7, LOW);

   Wire2.begin();
   Wire2.setClock(I2C_BUS_SPEED);
#ifdef ADS1xxx
   ads1014_reset();
#endif

#ifdef ECAT
   ecat_slv_init(&config);
#endif

#if 1                                // Uncomment for commissioning tests
   digitalWrite(outputPin[0], HIGH); // All four output leds should go high
   digitalWrite(outputPin[1], HIGH);
   digitalWrite(outputPin[2], HIGH);
   digitalWrite(outputPin[3], HIGH);
   while (1) // Apply voltage over the inputs 0-11 and see response in terminal
   {
      int nDevices = 0;
      for (int i2caddr = 1; i2caddr < 127; i2caddr++)
      {
         Wire2.beginTransmission(i2caddr);
         int stat = Wire2.endTransmission();
         if (stat == 0)
         {
            Serial1.printf("I2C device found at address 0x%02x\n", i2caddr);
            nDevices++;
         }
      }
      if (!nDevices)
         Serial1.printf("No devices\n");
#ifdef MCP3221
      Serial1.printf("I2C status=%d rawdata=%d ", mcp3221_0.ping(), mcp3221_0.getData());
#endif
#ifdef ADS1xxx
      //     else Serial1.printf("I2C status=%d rawdata=%d pin0=%d pin1=%d\n", ads1014.isConnected() ? 0 : -1, ads1014.readADC_Differential_0_1(), ads1014.readADC(0), ads1014.readADC(1));
      //    Serial1.println(ads1014.toVoltage(ads1014.readADC_Differential_0_1()), 5);
      for (int i = 0; i < 10; i++)
         Serial1.println(ads1014.getValue());
      int dummy = 0;
      uint32_t then = micros();
      for (int i = 0; i < 1000; i++)
         dummy += ads1014.getValue();
      uint32_t now = micros();
      Serial1.printf("1000 I2C readings take %d microseconds\n", now - then);
#endif
      for (int i = 0; i < 12; i++)
         Serial1.printf("%u", digitalRead(inputPin[i]));
      Serial1.println();
      delay(1000);
   }
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
