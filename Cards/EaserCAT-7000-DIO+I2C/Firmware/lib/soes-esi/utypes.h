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
   int32_t RawData;
   float CalculatedVoltage;
   uint8_t Status;
   float LowpassFilteredVoltage;
   uint8_t OhmicSensingSensed;

   /* Outputs */

   uint8_t Output4;
   float VoltageScale;
   float VoltageOffset;
   float LowPassFilterThresholdVoltage;
   uint8_t EnableOhmicSensing;
   uint32_t OhmicSensingSetupTime;
   float OhmicSensingVoltageLimit;

   /* Parameters */

   uint8_t I2C_devicetype;
   uint8_t I2C_address;
   uint32_t LowpassFilterPoleFrequency;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
