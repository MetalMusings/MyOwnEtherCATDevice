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

////// Digital IO
const byte INPUTS[8] = {PE8, PE9, PE10, PE11, PE12, PE13, PE14, PE15};
const byte OUTPUTS[4] = {PC5, PB0, PB1, PE7};
const byte THCAD_PIN = PA2; // CAN BE PA0, should be PA0
#define bitset(byte, nbit) ((byte) |= (1 << (nbit)))
#define bitclear(byte, nbit) ((byte) &= ~(1 << (nbit)))
#define bitflip(byte, nbit) ((byte) ^= (1 << (nbit)))
#define bitcheck(byte, nbit) ((byte) & (1 << (nbit)))

///// Analog out
const byte DAC1_pin = PA4;

//////// Stepper generators
#include "StepGen3.h"
StepGen3 *Step = 0;
HardwareTimer *baseTimer; // The base period timer
uint32_t sync0CycleTime;  // nanosecs, often 1000000 ( 1 ms )
volatile double posCmd1, posCmd2, posCmd3, posCmd4;
volatile float posScale1, posScale2, posScale3, posScale4;
volatile float maxAcc1, maxAcc2, maxAcc3, maxAcc4;
volatile uint8_t enable1, enable2, enable3, enable4;
volatile uint32_t basePeriod, newBasePeriod;
volatile uint16_t basePeriodCnt;
volatile uint16_t deltaMakePulsesCnt;
volatile uint64_t makePulsesCnt = 0;
void updateStepperGenerators(void);
void basePeriodCB(void);

///////// Spindle Encoder
#include "MyEncoder.h"
volatile uint16_t encCnt = 0;
void indexPulseEncoderCB1(void);
MyEncoder Encoder1(TIM2, PA3, indexPulseEncoderCB1);
void indexPulseEncoderCB1(void)
{
   encCnt++;
   Encoder1.indexPulse();
}

///////// Frequency counter for Torch height
#include "HardwareTimer.h"
// NOTE This mod in the beginning (line 33) of HardwareTimer.cpp for 32-bit precision
////// //#define MAX_RELOAD ((1 << 16) - 1) // Currently even 32b timers are used as 16b to have generic behavior
////// #define MAX_RELOAD 0xFFFFFFFF
////// HardwareTimer.cpp is part of the Stm32duino code <add where to find that>

uint32_t channel;
volatile uint32_t FrequencyMeasured, LastCapture = 0, CurrentCapture;
uint32_t input_freq = 0;
volatile uint32_t rolloverCompareCount = 0;
HardwareTimer *FrequencyTimer;
void InputCapture_IT_callback(void);
void Rollover_IT_callback(void);

////// EtherCAT
const byte SYNC0 = PC3;
const byte SYNC1 = PC1;
const byte SINT = PC0;
volatile uint16_t ALEventIRQ;                 // ALEvent that caused the interrupt
volatile byte serveIRQ = 0;                   // Flag indicating we got a SYNCx pulse and should act on that
volatile uint32_t globalIRQ = 0;              // Testing
extern "C" uint32_t ESC_SYNC0cycletime(void); // A SOES function we need
void globalInt(void);                         // ISR for INT line

////// EtherCAT routines called regularly, to read data, do stuff and write data
void cb_set_outputs(void) // Get Master outputs, slave inputs, first operation
{
   for (int i = 0; i < 4; i++)
      digitalWrite(OUTPUTS[i], bitcheck(Obj.Output4, i) ? HIGH : LOW);
   // analogWrite(DAC1_pin, Obj.Voltage);
   // Encoder1.setLatch(Obj.IndexLatchEnable);
   // Encoder1.setScale(2000);

   posScale1 = Obj.StepsPerMM1; // Scale perhaps changed
   posScale2 = Obj.StepsPerMM2;
   posScale3 = Obj.StepsPerMM3;
   posScale4 = Obj.StepsPerMM4;
   posCmd1 = Obj.CommandedPosition1; // The position update, etc
   posCmd2 = Obj.CommandedPosition2; // Is recognised by the base Time loop
   posCmd3 = Obj.CommandedPosition3;
   posCmd4 = Obj.CommandedPosition4;
   maxAcc1 = Obj.MaxAcceleration1;
   maxAcc2 = Obj.MaxAcceleration2;
   maxAcc3 = Obj.MaxAcceleration3;
   maxAcc4 = Obj.MaxAcceleration4;
   enable1 = Obj.Enable1;
   enable2 = Obj.Enable2;
   enable3 = Obj.Enable3;
   enable4 = Obj.Enable4;
   if (Obj.BasePeriod != 0) // Use default value from setup() if not set by SDO.
      newBasePeriod = Obj.BasePeriod;
}

void cb_get_inputs(void) // Set Master inputs, slave outputs, last operation
{
   Obj.Velocity = Obj.Scale * FrequencyMeasured;
   float scale = 1;
   if (Obj.Scale != 0.0)
      scale = Obj.Scale;
   Obj.Velocity = scale * sin(ESCvar.Time * 1e-8 * 6.28); // Test

   for (int i = 0; i < 8; i++)
      if (digitalRead(INPUTS[i]) == HIGH)
         bitSet(Obj.Input8, i);
      else
         bitClear(Obj.Input8, i);
#if 0
   Obj.IndexStatus = Encoder1.indexHappened();
   Obj.EncPos = Encoder1.currentPos();
   Obj.EncFrequency = Encoder1.frequency(longTime.extendTime(micros()));
   Obj.IndexByte = Encoder1.getIndexState();

   Obj.Velocity = Obj.Scale * FrequencyMeasured;
#endif
   Obj.ActualPosition1 = Step->stepgen_array[0].pos_fb;
   Obj.ActualPosition2 = Step->stepgen_array[1].pos_fb;
   Obj.ActualPosition3 = Step->stepgen_array[2].pos_fb;
   Obj.ActualPosition4 = Step->stepgen_array[3].pos_fb;
}

void handleStepper(void) // Called every cycle, updates stepper generator with new positions,
                         // restarts stepper generator and reads out current posution
{
   static int warned = 0;
   if (!warned && sync0CycleTime == 0) // This is kludge to be used during testing to activate stepper during free run
                                       // Stepper generators normally run only during synchronized conditions. But to do testing.
   {
      sync0CycleTime = 1000000; // 1e6 ns = 1e3 us = 1ms
      Serial1.println("Warn sync0Cycletime");
      warned = 1;
   }
   updateStepperGenerators();
}

void ESC_interrupt_enable(uint32_t mask);
void ESC_interrupt_disable(uint32_t mask);
uint16_t dc_checker(void);
void sync0Handler(void);

static esc_cfg_t config =
    {
        .user_arg = NULL,
        .use_interrupt = 0,
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

void setup(void)
{
   Serial1.begin(115200);
   delay(1000); // To make terminal window ready

   for (int i = 0; i < 4; i++)
   {
      pinMode(OUTPUTS[i], OUTPUT);
      digitalWrite(OUTPUTS[i], LOW);
   }
   for (int i = 0; i < 8; i++)
      pinMode(INPUTS[i], INPUT);
   pinMode(DAC1_pin, OUTPUT);
   analogWrite(DAC1_pin, 0);

   Step = new StepGen3;   // More settings in StepGen3.cpp and Stepgen3.h
   pinMode(PA11, OUTPUT); // Step 1
   pinMode(PA12, OUTPUT); // Dir 1
   pinMode(PC9, OUTPUT);  // Step 2
   pinMode(PC8, OUTPUT);  // Dir 2
   pinMode(PD12, OUTPUT); // Step 3
   pinMode(PD11, OUTPUT); // Dir 3
   pinMode(PE5, OUTPUT);  // Step 4
   pinMode(PE4, OUTPUT);  // Dir 4

   basePeriod = newBasePeriod = BASE_PERIOD; // Random-ish number, but it should work. Change through sdos

   baseTimer = new HardwareTimer(TIM11);                        // The base period timer
   baseTimer->setOverflow(BASE_PERIOD / 1000, MICROSEC_FORMAT); // Or the line above, This one is uncalibrated
   baseTimer->attachInterrupt(basePeriodCB);

   // Automatically retrieve TIM instance and channel associated to pin
   // This is used to be compatible with all STM32 series automatically.
   TIM_TypeDef *Instance = (TIM_TypeDef *)pinmap_peripheral(digitalPinToPinName(THCAD_PIN), PinMap_PWM);
   channel = STM_PIN_CHANNEL(pinmap_function(digitalPinToPinName(THCAD_PIN), PinMap_PWM));

   // FrequencyTimer = new HardwareTimer(Instance); // TIM2
   FrequencyTimer = new HardwareTimer(TIM5); // TIM5

   uint32_t PrescalerFactor = 1;
   FrequencyTimer->setMode(channel, TIMER_INPUT_CAPTURE_RISING, THCAD_PIN);
   FrequencyTimer->setPrescaleFactor(PrescalerFactor);
   FrequencyTimer->setOverflow(0xFFFFFFF0); // Max Period value to have the largest possible time to detect rising edge and avoid timer rollover
   FrequencyTimer->attachInterrupt(channel, InputCapture_IT_callback);
   FrequencyTimer->attachInterrupt(Rollover_IT_callback);
   FrequencyTimer->resume();
   // Compute this scale factor only once
   input_freq = FrequencyTimer->getTimerClkFreq() / FrequencyTimer->getPrescaleFactor();

   encoder_config(); // Needed by encoder, possibly breaks some timers.

   ecat_slv_init(&config);
   attachInterrupt(digitalPinToInterrupt(PC0), globalInt, RISING); // For testing, should go into Enable_interrupt later on
}

void loop(void)
{
   uint64_t dTime;
   if (serveIRQ)
   {
      DIG_process(DIG_PROCESS_WD_FLAG | DIG_PROCESS_OUTPUTS_FLAG |
                  DIG_PROCESS_APP_HOOK_FLAG | DIG_PROCESS_INPUTS_FLAG);
      serveIRQ = 0;
      ESCvar.PrevTime = ESCvar.Time;
      ecat_slv_poll();
   }
   dTime = longTime.extendTime(micros()) - irqTime;
   if (dTime > 5000) // Don't run ecat_slv_poll when expecting to serve interrupt
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
      attachInterrupt(digitalPinToInterrupt(SYNC0), sync0Handler, RISING);

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
   uint32_t user_int_mask = ESCREG_ALEVENT_DC_SYNC0 | ESCREG_ALEVENT_SM2 | ESCREG_ALEVENT_SM3;

   if (mask & user_int_mask)
   {
      // Disable interrupt from SYNC0 etc
      ESC_ALeventmaskwrite(ESC_ALeventmaskread() & ~(mask & user_int_mask));
      detachInterrupt(digitalPinToInterrupt(SYNC0));
      // Disable LAN9252 interrupt
      uint32_t bits = 0x00000000;
      ESC_write(0x5c, &bits, 4);
   }
}

// Setup of DC
uint16_t dc_checker(void)
{
   // Indicate we run DC
   ESCvar.dcsync = 1;
   sync0CycleTime = ESC_SYNC0cycletime(); // nanosecs
   return 0;
}

// Test/debug routine for the INT line
void globalInt(void)
{
   globalIRQ++;
}

////// Frequency counter (torch height) callback routines
void InputCapture_IT_callback(void)
{
   CurrentCapture = FrequencyTimer->getCaptureCompare(channel);

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

///// Stepper generator callback routines
void updateStepperGenerators(void)
{
   baseTimer->pause();
   Step->updateStepGen(posCmd1, posCmd2, posCmd3, posCmd4,
                       posScale1, posScale2, posScale3, posScale4,
                       maxAcc1, maxAcc2, maxAcc3, maxAcc4,
                       enable1, enable2, enable3, enable4,
                       sync0CycleTime); // Update positions
   Step->makeAllPulses();               // Make first step right here
   if (newBasePeriod != basePeriod)     // Changed via sdos
   {
      basePeriod = newBasePeriod;
      baseTimer->setOverflow(basePeriod / 1000, MICROSEC_FORMAT); // update timer frequency
   }
   basePeriodCnt = sync0CycleTime / basePeriod; //
   baseTimer->refresh();                        //
   baseTimer->resume();
   // Make the other steps in baseTimer's ISR
}

void basePeriodCB(void)
{
   if (--basePeriodCnt > 0)  // Stop
      Step->makeAllPulses(); // Make steps and pulses here
   else
      baseTimer->pause();
}
