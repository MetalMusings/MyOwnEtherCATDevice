#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

/* Object dictionary storage */

typedef struct
{
   /* Identity */

   uint32_t serial;

   /* Inputs */

   float EncPos;
   float EncFrequency;
   uint16_t DiffT;
   uint32_t IndexByte;
   uint32_t IndexStatus;
   float ActualPosition1;
   float ActualPosition2;
   int16_t D1;
   int16_t D2;
   int16_t D3;
   int16_t D4;

   /* Outputs */

   uint32_t IndexLatchEnable;
   float CommandedPosition1;
   float CommandedPosition2;
   int16_t StepsPerMM1;
   int16_t StepsPerMM2;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
