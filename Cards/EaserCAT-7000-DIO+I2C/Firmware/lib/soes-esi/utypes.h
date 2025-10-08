#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

/* Object dictionary storage */

typedef struct
{
   /* Identity */

   uint32_t serial;

   /* Inputs */

   int32_t Input12;
   struct
   {
      int32_t RawData;
      float CalculatedVoltage;
      uint32_t Status;
      float LowpassFilteredVoltage;
      uint8_t OhmicSensingSensed;
   } Out_Unit1;
   struct
   {
      int32_t RawData;
      float CalculatedVoltage;
      uint32_t Status;
      float LowpassFilteredVoltage;
      uint8_t OhmicSensingSensed;
   } Out_Unit2;

   /* Outputs */

   uint8_t Output4;
   struct
   {
      float VoltageScale;
      float VoltageOffset;
      float LowPassFilterThresholdVoltage;
      uint8_t EnableOhmicSensing;
      uint32_t OhmicSensingSetupTime;
      float OhmicSensingVoltageLimit;
      float OhmicSensingVoltageDrop;
   } In_Unit1;
   struct
   {
      float VoltageScale;
      float VoltageOffset;
      float LowPassFilterThresholdVoltage;
      uint8_t EnableOhmicSensing;
      uint32_t OhmicSensingSetupTime;
      float OhmicSensingVoltageLimit;
      float OhmicSensingVoltageDrop;
   } In_Unit2;

   /* Parameters */

   struct
   {
      uint8_t I2C_devicetype;
      uint8_t I2C_address;
      uint32_t LowpassFilterPoleFrequency;
   } Settings_Unit1;
   struct
   {
      uint8_t I2C_devicetype;
      uint8_t I2C_address;
      uint32_t LowpassFilterPoleFrequency;
   } Settings_Unit2;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
