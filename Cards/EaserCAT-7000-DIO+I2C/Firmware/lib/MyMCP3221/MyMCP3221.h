/*==============================================================================================================*

    @file     MyMCP3221.h
    @author   Nadav Matalon
    @license  MIT (c) 2016 Nadav Matalon

    MCP3221 Driver (12-BIT Single Channel ADC with I2C Interface)

    Ver. 1.0.0 - First release (16.10.16)

 *===============================================================================================================*
    INTRODUCTION
 *===============================================================================================================*

    The MCP3221 is a 12-Bit Single Channel ADC with hardware I2C interface.

    This library contains a complete driver for the MCP3221 allowing the user to get raw conversion data, 
    smoothed conversion data (Rollong-Average or EMAVG), or voltage readings ranging 0-5V or 0-12V (the latter
    requires a voltage divider setup).

 *===============================================================================================================*
    DEVICE HOOKUP
 *===============================================================================================================*

                                   MCP3221
                                   -------
                            VCC --| •     |-- SCL
                                  |       |
                            GND --|       |
                                  |       |
                            AIN --|       |-- SDA
                                   -------

    PIN 1 (VCC/VREF) - Serves as both Power Supply input and Voltage Reference for the ADC. Connect to the Arduino 
                       5V Output or any other equivalent power source (5.5V max). If using an external power source, 
                       remember to connect all GND's together
    PIN 2 (GND) - connect to Arduino GND
    PIN 3 (AIN) - Connect to Arduino's 3.3V Output or the middle pin of a 10K potentiometer (other two pins go to 5V & GND)
    PIN 4 (SDA) - Connect to Arduino's PIN A4 with a 2K2 (400MHz I2C Bus speed) or 10K (100MHz I2C Bus speed) pull-up resistor
    PIN 5 (SCL) - Connect to Arduino's PIN A5 with a 2K2 (400MHz I2C Bus speed) or 10K (100MHz I2C Bus speed) pull-up resistor
    DECOUPING:    Minimal decoupling consists of a 0.1uF Ceramic Capacitor between the VCC & GND PINS. For improved 
                  performance, add a 1uF and a 10uF Ceramic Capacitors as well across these pins

 *===============================================================================================================*
    VOLTAGE DIVIDER HOOKUP (OPTIONAL: FOR 12V READINGS)
 *===============================================================================================================*

                      12V
                       |            MCP3221
                       |            -------
                   R1 | |          |       |
                      | |          |       |
                       |       AIN |       |
                       |-----------|       |
                       |           |       |
                      | |           -------
                   R2 | |
                       |
                       |
                      GND
 
                        R1 - 10K Resistor
                        R2 - 4K7 Resistor
 
 *===============================================================================================================*
    I2C ADDRESSES
 *===============================================================================================================*

    Each MCP3221 has 1 of 8 possible I2C addresses (factory hardwired & recognized by its specific
    part number & top marking on the package itself):

         PART              DEVICE I2C ADDRESS         PART
        NUMBER         (BIN)      (HEX)     (DEC)    MARKING
    MCP3221A0T-E/OT   01001000    0x48       72        GE
    MCP3221A1T-E/OT   01001001    0x49       73        GH
    MCP3221A2T-E/OT   01001010    0x4A       74        GB
    MCP3221A3T-E/OT   01001000    0x4B       75        GC
    MCP3221A4T-E/OT   01001100    0x4C       76        GD
    MCP3221A5T-E/OT   01001101    0x4D       77        GA
    MCP3221A6T-E/OT   01001110    0x4E       78        GF
    MCP3221A7T-E/OT   01001111    0x4F       79        GG

 *===============================================================================================================*
    DEVICE SETTING DEFAULTS
 *===============================================================================================================*

    VOLTAGE REFERENCE:           4096mV  // this value is equal to the voltage fed to VCC
    VOLTAGE INPUT:                  5V   // direct measurment of voltage at AIN pin (hw setup without voltage divider)
    VOLTAGE DIVIDER RESISTOR 1:     0R   // value used when measuring voltage of up to 12V at AIN pin
    VOLTAGE DIVIDER RESISTOR 2:     0R   // value used when measuring voltage of up to 12V at AIN pin
    NUMBER OF SAMPLES:              10   // used by Rolling-Average smoothing method (range: 1-20 Samples)
    ALPHA                          178   // factor used by EMAVG smoothing method (range: 1-256)

 *===============================================================================================================*
    BUG REPORTS
 *===============================================================================================================*

    Please report any bugs/issues/suggestions at the Github Repo of this library at: 
    https://github.com/nadavmatalon/MCP3221
 
 *===============================================================================================================*
    LICENSE
 *===============================================================================================================*

    The MIT License (MIT)
    Copyright (c) 2016 Nadav Matalon

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
    documentation files (the "Software"), to deal in the Software without restriction, including without
    limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial
    portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
    LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 *==============================================================================================================*/

 #ifndef MyMCP3221_h
 #define MyMCP3221_h

 #include <Arduino.h>
 #include <Wire.h>
 //#include "utility/MCP3221_PString.h"
 
 namespace Mymcp3221 {
     
     const byte         DATA_BYTES          =     2;     // number of data bytes requested from the device
     const byte         MIN_CON_TIME        =    15;     // single conversion time with a small overhead (in uS)
     const byte         COM_SUCCESS         =     0;     // I2C communication success Code (No Error)
     const unsigned int MIN_VREF            =  2700;     // minimum Voltage Reference value in mV (same as VCC)
     const unsigned int MAX_VREF            =  5500;     // minimum Voltage Reference value in mV (same as VCC)
     const unsigned int DEFAULT_VREF        =  4096;     // default Voltage Reference value in mV (same as VCC)
     const unsigned int DEFAULT_RES_1       = 10000;     // default Resistor 1 value (in Ω) of voltage divider for 12V readings
     const unsigned int DEFAULT_RES_2       =  4700;     // default Resistor 2 value (in Ω) of voltage divider for 12V readings
     const unsigned int MIN_ALPHA           =     1;     // minimum value of alpha (slowest change) (for EMAVG)
     const unsigned int MAX_ALPHA           =   256;     // maximum value of alpha (raw change/no filter) (for EMAVG)
     const unsigned int DEFAULT_ALPHA       =   178;     // default value of alpha (for EMAVG)
     const byte         MIN_NUM_SAMPLES     =     1;     // minimum number of samples (for Rolling-Average smoothing)
     const byte         MAX_NUM_SAMPLES     =    20;     // maximum number of samples (for Rolling-Average smoothing)
     const byte         DEFAULT_NUM_SAMPLES =    10;     // default number of samples (for Rolling-Average smoothing)
 
     typedef enum:byte {
         VOLTAGE_INPUT_5V  = 0,  // default
         VOLTAGE_INPUT_12V = 1
     } voltage_input_t;
 
     typedef enum:byte {
         NO_SMOOTHING = 0,
         ROLLING_AVG  = 1,
         EMAVG        = 2     // Default
     } smoothing_t;
 
     class MyMCP3221 {
         public:
             MyMCP3221(
                     byte devAddr,
                     TwoWire *Wire,
                     unsigned int    vRef            = DEFAULT_VREF,
                     unsigned int    res1            = DEFAULT_RES_1,
                     unsigned int    res2            = DEFAULT_RES_2,
                     unsigned int    alpha           = DEFAULT_ALPHA,
                     voltage_input_t voltageInput    = VOLTAGE_INPUT_5V,
                     smoothing_t     smoothingMethod = EMAVG,
                     byte            numSamples      = DEFAULT_NUM_SAMPLES
                    );
             ~MyMCP3221();
             byte ping();
             unsigned int getVref();
             unsigned int getRes1();
             unsigned int getRes2();
             unsigned int getAlpha();
             byte         getNumSamples();
             byte         getVinput();
             byte         getSmoothing();
             unsigned int getData();
             unsigned int getVoltage();
             byte         getComResult();
             void         setVref(unsigned int newVref);
             void         setRes1(unsigned int newRes1);
             void         setRes2(unsigned int newRes2);
             void         setAlpha(unsigned int newAlpha);
             void         setNumSamples(byte newNumSamples);
             void         setVinput(voltage_input_t newVinput);
             void         setSmoothing(smoothing_t newSmoothing);
             void         reset();
         private:
             TwoWire      *_Wire;
             byte         _devAddr, _voltageInput, _smoothing, _numSamples, _comBuffer;
             unsigned int _vRef, _res1, _res2, _alpha;
             unsigned int _samples[MAX_NUM_SAMPLES];
             unsigned int getRawData();
             unsigned int smoothData(unsigned int rawData);
    //         friend       MyMCP3221_PString MyMCP3221ComStr(const MyMCP3221&);
    //         friend       MyMCP3221_PString MyMCP3221InfoStr(const MyMCP3221&);
     };
 }
 
 using namespace Mymcp3221;
 
 #endif