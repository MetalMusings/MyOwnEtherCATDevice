#include <Arduino.h>
#include <stdio.h>
extern "C" {
#include "ecat_slv.h"
#include "utypes.h"
};
_Objects Obj;

#include "extend32to64.h"
extend32to64 longTime;
volatile uint64_t irqTime = 0;

HardwareSerial Serial1(PA10, PA9);

uint8_t inputPin[] = {PD15, PD14, PD13, PD12, PD11, PD10,
                      PD9,  PD8,  PB15, PB14, PB13, PB12};
uint8_t outputPin[] = {PE10, PE9, PE8, PE7};

const uint32_t I2C_BUS_SPEED = 400000;
uint32_t I2C_restarts_1 = 0, I2C_restarts_2 = 0;
const uint8_t MCP3221_TYPE = 1, ADS1014_TYPE = 2;
int8_t old_I2Cdevice_1 = -1;
int8_t old_I2Cdevice_2 = -1;

#include "Wire.h"
TwoWire Wire2(PB11, PB10);

#include "MyMCP3221.h"
MyMCP3221 *mcp3221_1 = 0;
MyMCP3221 *mcp3221_2 = 0;

#include "ADS1X15.h"
ADS1014 *ads1014_1 = 0;
ADS1014 *ads1014_2 = 0;

void ads1014_reset(ADS1014 *ads) {
  ads->reset();
  ads->begin();
  ads->setGain(1);                  // 1=4.096V
  ads->setMode(0);                  // 0 continuous
  ads->setDataRate(6);              // Max for ads101x
  ads->readADC_Differential_0_1();  // This is the value we are interested in
}

#include <queue>
class OhmicSensing {
 public:
  void handle(uint8_t voltageState, float inVoltage, float limitVoltage,
              float voltageDropLimit, uint32_t setupTime, uint8_t enabled,
              uint8_t &sensed);

  // private:
  enum OhmicStates {
    OHMIC_IDLE,
    OHMIC_SETUP,
    OHMIC_PROBE,
    OHMIC_PROBED_ON,
    OHMIC_PROBE_DONE
  };
  OhmicStates ohmicState = OHMIC_IDLE;
  uint32_t setupTimeSoFar = 0;
  uint32_t probingTime = 0;
  uint16_t senseOnTime = 0;
  float_t oldVoltage = 0.0;
  std::queue<float> voltages;
  float_t refVoltage;
};
OhmicSensing Ohm1;
OhmicSensing Ohm2;

void handleVoltageReader(float scale_in, float offset, float &outVoltage,
                         int32_t &outRaw, float &oldVoltage, float &oldRaw,
                         uint8_t devType, int8_t &old_devType,
                         uint8_t &readStat, uint32_t &outStatus, ADS1014 *&ads,
                         MyMCP3221 *&mcp, uint8_t I2C_address,
                         uint32_t &I2C_restarts);
void lowpassFilter(float &oldLowPassGain,
                   uint32_t &oldLowpassFilterPoleFrequency,
                   float &oldLowPassFilteredVoltage,
                   uint32_t LowpassFilterPoleFrequency,
                   float LowPassFilterThresholdVoltage, float inVoltage,
                   float &outFilteredVoltage);

#define bitset(byte, nbit) ((byte) |= (1 << (nbit)))
#define bitclear(byte, nbit) ((byte) &= ~(1 << (nbit)))
#define bitflip(byte, nbit) ((byte) ^= (1 << (nbit)))
#define bitcheck(byte, nbit) ((byte) & (1 << (nbit)))

volatile uint16_t ALEventIRQ;  // ALEvent that caused the interrupt
extern "C" uint32_t ESC_SYNC0cycletime(void);

void cb_set_outputs(void)  // Get Master outputs, slave inputs, first operation
{
  // Update digital output pins
  for (int i = 0; i < sizeof(outputPin); i++)
    digitalWrite(outputPin[i], bitcheck(Obj.Output4, i) ? HIGH : LOW);
}

float oldLowPassGain_1 = 0, oldLowPassGain_2 = 0;
float oldLowPassFilteredVoltage_1 = 0, oldLowPassFilteredVoltage_2 = 0;
uint32_t oldLowpassFilterPoleFrequency_1 = 0,
         oldLowpassFilterPoleFrequency_2 = 0;

void cb_get_inputs(void)  // Set Master inputs, slave outputs, last operation
{
  static float validData0_1 = 0.0, validVoltage0_1 = 0.0;
  static float validData0_2 = 0.0, validVoltage0_2 = 0.0;
  uint8_t stat_1, stat_2;

  for (int i = 0; i < sizeof(inputPin); i++)
    Obj.Input12 = digitalRead(inputPin[i]) == HIGH ? bitset(Obj.Input12, i)
                                                   : bitclear(Obj.Input12, i);

  handleVoltageReader(Obj.In_Unit1.VoltageScale, Obj.In_Unit1.VoltageOffset,
                      Obj.Out_Unit1.CalculatedVoltage, Obj.Out_Unit1.RawData,
                      validVoltage0_1, validData0_1,
                      Obj.Settings_Unit1.I2C_devicetype, old_I2Cdevice_1,
                      stat_1, Obj.Out_Unit1.Status, ads1014_1, mcp3221_1,
                      Obj.Settings_Unit1.I2C_address, I2C_restarts_1);
  handleVoltageReader(Obj.In_Unit2.VoltageScale, Obj.In_Unit2.VoltageOffset,
                      Obj.Out_Unit2.CalculatedVoltage, Obj.Out_Unit2.RawData,
                      validVoltage0_2, validData0_2,
                      Obj.Settings_Unit2.I2C_devicetype, old_I2Cdevice_2,
                      stat_2, Obj.Out_Unit2.Status, ads1014_2, mcp3221_2,
                      Obj.Settings_Unit2.I2C_address, I2C_restarts_2);
  lowpassFilter(oldLowPassGain_1, oldLowpassFilterPoleFrequency_1,
                oldLowPassFilteredVoltage_1,
                Obj.Settings_Unit1.LowpassFilterPoleFrequency,
                Obj.In_Unit1.LowPassFilterThresholdVoltage,
                Obj.Out_Unit1.CalculatedVoltage,
                Obj.Out_Unit1.LowpassFilteredVoltage);
  lowpassFilter(oldLowPassGain_2, oldLowpassFilterPoleFrequency_2,
                oldLowPassFilteredVoltage_2,
                Obj.Settings_Unit2.LowpassFilterPoleFrequency,
                Obj.In_Unit2.LowPassFilterThresholdVoltage,
                Obj.Out_Unit2.CalculatedVoltage,
                Obj.Out_Unit2.LowpassFilteredVoltage);

  Ohm1.handle(
      stat_1, Obj.Out_Unit1.CalculatedVoltage,
      Obj.In_Unit1.OhmicSensingVoltageLimit,
      Obj.In_Unit1.OhmicSensingVoltageDrop, Obj.In_Unit1.OhmicSensingSetupTime,
      Obj.In_Unit1.EnableOhmicSensing, Obj.Out_Unit1.OhmicSensingSensed);
  Ohm2.handle(
      stat_2, Obj.Out_Unit2.CalculatedVoltage,
      Obj.In_Unit2.OhmicSensingVoltageLimit,
      Obj.In_Unit2.OhmicSensingVoltageDrop, Obj.In_Unit2.OhmicSensingSetupTime,
      Obj.In_Unit2.EnableOhmicSensing, Obj.Out_Unit2.OhmicSensingSensed);
}

void ESC_interrupt_enable(uint32_t mask);
void ESC_interrupt_disable(uint32_t mask);
uint16_t dc_checker(void);
void sync0Handler(void);

static esc_cfg_t config = {
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

void setup(void) {
  Serial1.begin(115200);

  for (int i = 0; i < sizeof(inputPin); i++)
    pinMode(inputPin[i], INPUT_PULLDOWN);
  for (int i = 0; i < sizeof(outputPin); i++) {
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

#ifdef ECAT
  ecat_slv_init(&config);
#endif

#if 0  // Uncomment for commissioning tests
// #define only one of the below
#define ADS1xxx
#undef ADC_MCP3221
   digitalWrite(outputPin[0], HIGH); // All four output leds should go high
   digitalWrite(outputPin[1], HIGH);
   digitalWrite(outputPin[2], HIGH);
   digitalWrite(outputPin[3], HIGH);
#ifdef ADC_MCP3221
   mcp3221 = new MyMCP3221(0x48, &Wire2);
#endif
#ifdef ADS1xxx
   ads1014_1 = new ADS1014(0x48, &Wire2);
   ads1014_reset(ads1014_1);
#endif
   while (1) // Search I2C bus for devices
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
#ifdef ADC_MCP3221
      Serial1.printf("I2C status=%d rawdata=%d ", mcp3221->ping(), mcp3221->getData());
#endif
#ifdef ADS1xxx
      //     else Serial1.printf("I2C status=%d rawdata=%d pin0=%d pin1=%d\n", ads1014.isConnected() ? 0 : -1, ads1014.readADC_Differential_0_1(), ads1014.readADC(0), ads1014.readADC(1));
      //    Serial1.println(ads1014.toVoltage(ads1014.readADC_Differential_0_1()), 5);
      for (int i = 0; i < 10; i++)
         Serial1.println(ads1014_1->getValue());
      int dummy = 0;
      uint32_t then = micros();
      for (int i = 0; i < 1000; i++)
         dummy += ads1014_1->getValue();
      uint32_t now = micros();
      Serial1.printf("1000 I2C readings take %d microseconds\n", now - then);
      Serial1.println(ads1014_1->toVoltage(ads1014_1->getValue()), 4);
#endif
      for (int i = 0; i < 12; i++)
         Serial1.printf("%u", digitalRead(inputPin[i]));
      Serial1.println();
      delay(1000);
   }
#endif
}

void loop(void) {
#ifdef ECAT
  uint64_t dTime;
  if (serveIRQ) {
    DIG_process(ALEventIRQ, DIG_PROCESS_WD_FLAG | DIG_PROCESS_OUTPUTS_FLAG |
                                DIG_PROCESS_APP_HOOK_FLAG |
                                DIG_PROCESS_INPUTS_FLAG);
    serveIRQ = 0;
    ESCvar.PrevTime = ESCvar.Time;
    ecat_slv_poll();
  }
  dTime = longTime.extendTime(micros()) - irqTime;
  if (dTime > 5000)  // Not doing interrupts - handle free-run
    ecat_slv();

#endif
}

void sync0Handler(void) {
  ALEventIRQ = ESC_ALeventread();
  // if (ALEventIRQ & ESCREG_ALEVENT_SM2)
  {
    irqTime = longTime.extendTime(micros());
    serveIRQ = 1;
  }
}

// Enable SM2 interrupts
void ESC_interrupt_enable(uint32_t mask) {
  // Enable interrupt for SYNC0 or SM2 or SM3
  uint32_t user_int_mask =
      ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 | ESCREG_ALEVENT_SM3;
  if (mask & user_int_mask) {
    ESC_ALeventmaskwrite(ESC_ALeventmaskread() | (mask & user_int_mask));
    ESC_ALeventmaskwrite(ESC_ALeventmaskread() &
                         ~(ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM3));
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
void ESC_interrupt_disable(uint32_t mask) {
  // Enable interrupt for SYNC0 or SM2 or SM3
  // uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 |
  // ESCREG_ALEVENT_SM3;
  uint32_t user_int_mask = ESCREG_ALEVENT_SM2;

  if (mask & user_int_mask) {
    // Disable interrupt from SYNC0
    ESC_ALeventmaskwrite(ESC_ALeventmaskread() & ~(mask & user_int_mask));
    detachInterrupt(digitalPinToInterrupt(PC3));
    // Disable LAN9252 interrupt
    uint32_t bits = 0x00000000;
    ESC_write(0x5c, &bits, 4);
  }
}

// Setup of DC
uint16_t dc_checker(void) {
  // Indicate we run DC
  ESCvar.dcsync = 1;
  return 0;
}

void handleVoltageReader(float scale_in, float offset, float &outVoltage,
                         int32_t &outRaw, float &oldVoltage, float &oldRaw,
                         uint8_t devType, int8_t &old_devType,
                         uint8_t &readStat, uint32_t &outStatus, ADS1014 *&ads,
                         MyMCP3221 *&mcp, uint8_t I2C_address,
                         uint32_t &I2C_restarts) {
  float scale = scale_in;
  if (scale == 0.0) scale = 1.0;
  int stat = 1, data0;

  switch (devType) {
    case 0:  // Not configured.
      outStatus = 0;
      stat = data0 = 0;
      break;
    case MCP3221_TYPE:
      if (old_devType != devType)  // Initilize and make ready
      {
        if (ads) {
          delete ads;
          ads = 0;
        }
        if (mcp) {
          delete mcp;
          mcp = 0;
        }
        Wire2.end();
        Wire2.begin();
        Wire2.setClock(I2C_BUS_SPEED);
        mcp = new MyMCP3221(I2C_address, &Wire2);
        old_devType = mcp ? MCP3221_TYPE : -1;
      }
      data0 = mcp->getData();
      stat = mcp->ping();
      break;
    case ADS1014_TYPE:
      if (old_devType != devType)  // Initilize and make ready
      {
        if (ads) {
          delete ads;
          ads = 0;
        }
        if (mcp) {
          delete mcp;
          mcp = 0;
        }
        old_devType = 0;

        Wire2.end();
        Wire2.begin();
        Wire2.setClock(I2C_BUS_SPEED);
        ads = new ADS1014(I2C_address, &Wire2);
        if (ads != nullptr) {
          ads1014_reset(ads);
          old_devType = ADS1014_TYPE;
        }
      }
      if (ads != nullptr) {
        data0 = ads->getValue();
        stat = ads->isConnected() == 1 ? 0 : 1;
      }
      break;
    default:  // Not supported
      break;
  }

  if (stat == 0) {                        // Read good value
    outVoltage = scale * data0 + offset;  //
    outRaw = data0;                       // Raw voltage, read by ADC
    oldVoltage = outVoltage;
    oldRaw = data0;
  } else {  // Didn't read a good value. Return a hopefully useful value and
            // restart
            // the I2C bus
    outVoltage = oldVoltage;  // Use value from previous call
    outRaw = oldRaw;
    // Reset wire here
    Wire2.end();
    Wire2.begin();
    Wire2.setClock(I2C_BUS_SPEED);
    I2C_restarts++;
    if (devType == ADS1014_TYPE && ads != nullptr) ads1014_reset(ads);
    // mcp3221 has no reset, reset the I2C bus is the best we can do
  }
  readStat = stat;
  outStatus =
      I2C_restarts + (stat << 28);  // Put status as bits 28-31, the lower are
                                    // number of restarts (restart attempts)
}

void lowpassFilter(float &oldLowPassGain,
                   uint32_t &oldLowpassFilterPoleFrequency,
                   float &oldLowPassFilteredVoltage,
                   uint32_t LowpassFilterPoleFrequency,
                   float LowPassFilterThresholdVoltage, float inVoltage,
                   float &outFilteredVoltage) {
  // Low pass filter. See lowpass in linuxcnc doc
  float gain = oldLowPassGain;
  if (oldLowpassFilterPoleFrequency != LowpassFilterPoleFrequency) {
    gain = 1 - expf(-2.0 * M_PI * LowpassFilterPoleFrequency *
                    0.001 /*1.0e-9 * ESC_SYNC0cycletime()*/);
    oldLowPassGain = gain;
    oldLowpassFilterPoleFrequency = LowpassFilterPoleFrequency;
  }
  if (inVoltage < LowPassFilterThresholdVoltage)
    outFilteredVoltage = inVoltage;  // Just forward
  else
    outFilteredVoltage = oldLowPassFilteredVoltage +
                         (inVoltage - oldLowPassFilteredVoltage) * gain;
  oldLowPassFilteredVoltage = outFilteredVoltage;
}

#define N_VOLTAGES 3
void OhmicSensing::handle(uint8_t voltageState, float inVoltage,
                          float limitVoltage, float voltageDropLimit,
                          uint32_t setupTime, uint8_t enabled,
                          uint8_t &sensed) {
  sensed = 0;
  if (enabled && voltageState == 0) {
    if (ohmicState == OHMIC_IDLE && inVoltage > limitVoltage) {
      ohmicState = OHMIC_SETUP;
      setupTimeSoFar = 0;
      while (!voltages.empty()) voltages.pop();  // Remove history
      return;
    }
    if (ohmicState == OHMIC_SETUP) {
      if (setupTimeSoFar++ > setupTime) {
        ohmicState = OHMIC_PROBE;
        probingTime = 0;
        oldVoltage = 0.0;
        refVoltage = inVoltage;  // RefVoltage = voltage at end of setup
        return;
      }
    }
    if (ohmicState == OHMIC_PROBE) {
      voltages.push(inVoltage);
      while (voltages.size() > N_VOLTAGES) voltages.pop();  // Only N_VOLTAGES
      if (probingTime++ > 30000) {  // Go to IDLE after 30 seconds
        ohmicState = OHMIC_IDLE;
        return;
      }
      if ((inVoltage <= limitVoltage) ||  // Below starting threshold
          (fabs(voltageDropLimit) > 1e-3 &&
           refVoltage - inVoltage >=
               voltageDropLimit) ||          // Delta below refVoltage
          (fabs(voltageDropLimit) > 1e-3 &&  // Immediate drop
           oldVoltage - inVoltage >= voltageDropLimit) ||
          (fabs(voltageDropLimit) > 1e-3 &&  // Drop over 3 cycles
           voltages.front() - voltages.back() > voltageDropLimit)) {
        sensed = 1;
        senseOnTime = 0;
        ohmicState = OHMIC_PROBED_ON;
      }
      oldVoltage = inVoltage;
      return;
    }
    if (ohmicState == OHMIC_PROBED_ON) {
      sensed = 1;
      if (senseOnTime++ >= 100) {
        sensed = 0;
        ohmicState = OHMIC_PROBE_DONE;
      }
      return;
    }
    if (ohmicState == OHMIC_PROBE_DONE) {
      sensed = 0;
      if (!enabled) ohmicState = OHMIC_IDLE;
      return;
    }
  } else {
    ohmicState = OHMIC_IDLE;
  }
}
