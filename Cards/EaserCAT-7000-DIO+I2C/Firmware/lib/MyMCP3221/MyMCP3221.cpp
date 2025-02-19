/*==============================================================================================================*
 
    @file     MyMCP3221.cpp
    @author   Nadav Matalon
    @license  MIT (c) 2016 Nadav Matalon

    MCP3221 Driver (12-BIT Single Channel ADC with I2C Interface)

    Ver. 1.0.0 - First release (16.10.16)

 *==============================================================================================================*
    LICENSE
 *==============================================================================================================*
 
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
 
 #include "MyMCP3221.h"
 
 /*==============================================================================================================*
     CONSTRUCTOR
  *==============================================================================================================*/
 
 MyMCP3221::MyMCP3221(
      byte            devAddr,
      TwoWire         *Wire,
      unsigned int    vRef,
      unsigned int    res1,
      unsigned int    res2,
      unsigned int    alpha,
      voltage_input_t voltageInput,
      smoothing_t     smoothingMethod,
      byte            numSamples) :
      _Wire(Wire),
      _devAddr(devAddr),
      _vRef(vRef),
      _alpha(alpha),
      _voltageInput(voltageInput),
      _smoothing(smoothingMethod),
      _numSamples(numSamples)
      {
         memset(_samples, 0, sizeof(_samples));
         if (((res1 != 0) && (res2 != 0)) && (_voltageInput == VOLTAGE_INPUT_12V)) {
             _res1 = res1;
             _res2 = res2;
         } else if (_voltageInput == VOLTAGE_INPUT_5V) {
             _res1 = 0;
             _res2 = 0;
         } else {
             _res1 = DEFAULT_RES_1;
             _res2 = DEFAULT_RES_2;
     }
     _comBuffer = COM_SUCCESS;
 }
 
 /*==============================================================================================================*
     DESTRUCTOR
  *==============================================================================================================*/
 
 MyMCP3221::~MyMCP3221() {}
 
 /*==============================================================================================================*
     PING (0 = SUCCESS / 1, 2, ... = ERROR CODE)
  *==============================================================================================================*/
 
 // See meaning of I2C Error Code values in README
 
 byte MyMCP3221::ping() {
     _Wire->beginTransmission(_devAddr);
     return _comBuffer = _Wire->endTransmission();
 }
 
 /*==============================================================================================================*
     GET VOLTAGE REFERENCE (2700mV - 5500mV)
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getVref() {
     return _vRef;
 }
 
 /*==============================================================================================================*
     GET VOLTAGE DIVIDER RESISTOR 1 (Ω)
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getRes1() {
     return _res1;
 }
 
 /*==============================================================================================================*
     GET VOLTAGE DIVIDER RESISTOR 2 (Ω)
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getRes2() {
     return _res2;
 }
 
 /*==============================================================================================================*
     GET ALPHA (RELEVANT ONLY FOR EMAVG SMOOTHING METHOD, RANGE: 1 - 256)
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getAlpha() {
     return _alpha;
 }
 
 /*==============================================================================================================*
     GET NUMBER OF SAMPLES (RELEVANT ONLY FOR ROLLING-AVAREGE SMOOTHING METHOD, RANGE: 1-20 SAMPLES)
  *==============================================================================================================*/
 
 byte MyMCP3221::getNumSamples() {
     return _numSamples;
 }
 
 /*==============================================================================================================*
     GET VOLTAGE INPUT (0 = VOLTAGE_INPUT_5V / 1 = VOLTAGE_INPUT_12V)
  *==============================================================================================================*/
 
 byte MyMCP3221::getVinput() {
     return _voltageInput;
 }
 
 /*==============================================================================================================*
     GET SMOOTHING METHOD (0 = NONE / 1 = ROLLING-AVAREGE / 2 = EMAVG)
  *==============================================================================================================*/
 
 byte MyMCP3221::getSmoothing() {
     return _smoothing;
 }
 
 /*==============================================================================================================*
     GET DATA
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getData() {
     return (_smoothing == NO_SMOOTHING) ? getRawData() : smoothData(getRawData());
 }
 
 /*==============================================================================================================*
     GET VOLTAGE  (Vref 4.096V: 2700 - 4096mV)
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getVoltage() {
     if (_voltageInput == VOLTAGE_INPUT_5V) return round((_vRef / (float)DEFAULT_VREF) * getData());
     else return round(getData() * ((float)(_res1 + _res2) / _res2));
 }
 
 /*==============================================================================================================*
     GET LATEST I2C COMMUNICATION RESULT (0 = OK / 1, 2, ... = ERROR)
  *==============================================================================================================*/
 
 byte MyMCP3221::getComResult() {
     return _comBuffer;
 }
 
 /*==============================================================================================================*
     SET REFERENCE VOLTAGE (2700mV - 5500mV)
  *==============================================================================================================*/
 
 void MyMCP3221::setVref(unsigned int newVref) {                                  // PARAM RANGE: 2700-5500
     newVref = constrain(newVref, MIN_VREF, MAX_VREF);
     _vRef = newVref;
 }
 
 /*==============================================================================================================*
     SET VOLTAGE DIVIDER RESISTOR 1 (Ω)
  *==============================================================================================================*/
 
 void MyMCP3221::setRes1(unsigned int newRes1) {
     _res1 = newRes1;
 }
 
 /*==============================================================================================================*
     SET VOLTAGE DIVIDER RESISTOR 2 (Ω)
  *==============================================================================================================*/
 
 void MyMCP3221::setRes2(unsigned int newRes2) {
     _res2 = newRes2;
 }
 
 /*==============================================================================================================*
     SET ALPHA (RELEVANT ONLY FOR EMAVG SMOOTHING METHOD)
  *==============================================================================================================*/
 
 void MyMCP3221::setAlpha(unsigned int newAlpha) {                                      // PARAM RANGE: 1-256
     newAlpha = constrain(newAlpha, MIN_ALPHA, MAX_ALPHA);
     _alpha = newAlpha;
 }
 
 /*==============================================================================================================*
     SET NUMBER OF SAMPLES (RELEVANT ONLY FOR ROLLING-AVAREGE SMOOTHING METHOD)
  *==============================================================================================================*/
 
 void MyMCP3221::setNumSamples(byte newNumSamples) {                                    // PARAM RANGE: 1-20
     newNumSamples = constrain(newNumSamples, MIN_NUM_SAMPLES, MAX_NUM_SAMPLES);
     _numSamples = newNumSamples;
     for (byte i=0; i<MAX_NUM_SAMPLES; i++) _samples[i] = 0;
 }
 
 /*==============================================================================================================*
     SET VOLTAGE INPUT (NOTE: 12V INPUT READINGS REQUIRE A HARDWARE VOLTAGE DIVIDER)
  *==============================================================================================================*/
 
 void MyMCP3221::setVinput(voltage_input_t newVoltageInput) {     // PARAMS: VOLTAGE_INPUT_5V / VOLTAGE_INPUT_12V
     _voltageInput = newVoltageInput;
     if (newVoltageInput == VOLTAGE_INPUT_12V) {
         if (!_res1) _res1 = DEFAULT_RES_1;
         if (!_res2) _res2 = DEFAULT_RES_2;
     }
 }
 
 /*==============================================================================================================*
     SET SMOOTHING METHOD
  *==============================================================================================================*/
 
 void MyMCP3221::setSmoothing(smoothing_t newSmoothing) {           // PARAMS: NO_SMOOTHING / ROLLING / EMAVG
     _smoothing = newSmoothing;
 }
 
 /*==============================================================================================================*
     RESET
  *==============================================================================================================*/
 
 void MyMCP3221::reset() {
     setVref(DEFAULT_VREF);
     setAlpha(DEFAULT_ALPHA);
     setVinput(VOLTAGE_INPUT_5V);
     setSmoothing(EMAVG);
     setRes1(0);
     setRes2(0);
     setNumSamples(DEFAULT_NUM_SAMPLES);
 }
 
 /*==============================================================================================================*
     GET RAW DATA
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::getRawData() {
     unsigned int rawData = 0;
     _Wire->requestFrom(_devAddr, DATA_BYTES);
     if (_Wire->available() == DATA_BYTES) rawData = (_Wire->read() << 8) | (_Wire->read());
     else ping();
     return rawData;
 }
 
 /*==============================================================================================================*
     SMOOTH DATA
  *==============================================================================================================*/
 
 unsigned int MyMCP3221::smoothData(unsigned int rawData) {
     unsigned int smoothedData;
     if (_smoothing == EMAVG) {                                                  // Exmponential Moving Average
         unsigned int emAvg = rawData;
         emAvg = (_alpha * (unsigned long)rawData + (MAX_ALPHA - _alpha) * (unsigned long)emAvg) / MAX_ALPHA;
         smoothedData = emAvg;
     } else {                                                                    // Rolling-Average
         unsigned long sum = 0;
         if (_samples[_numSamples - 1] != 0) {
             for (byte i = 1; i<_numSamples; i++) _samples[i - 1] = _samples[i]; // drop last reading & rearrange array
             _samples[_numSamples - 1] = rawData;                                // add a new sample at the end of array
             for (byte j=0; j<_numSamples; j++) sum += _samples[j];              // aggregate all samples
             smoothedData = sum / _numSamples;                                   // calculate average
         } else {
             for (byte i = 0; i<_numSamples; i++) _samples[i] = rawData;
             smoothedData = rawData;
         }
     }
     return smoothedData;
 }
 