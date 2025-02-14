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

   /* Parameters */

   float Scale;
   float Offset;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
