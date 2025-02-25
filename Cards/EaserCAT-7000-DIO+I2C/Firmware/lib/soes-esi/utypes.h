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

   /* Outputs */

   uint8_t Output4;
   float VoltageScale;
   float VoltageOffset;

   /* Parameters */

   uint8_t I2C_devicetype;
   uint8_t I2C_address;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
