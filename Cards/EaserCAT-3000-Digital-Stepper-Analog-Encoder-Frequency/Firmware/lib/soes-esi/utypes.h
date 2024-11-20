#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

/* Object dictionary storage */

typedef struct
{
   /* Identity */

   uint32_t serial;

   /* Inputs */

   float Velocity;
   uint8_t Inputs[8];
   float Frequency;
   float ActualPosition1;
   float ActualPosition2;
   float ActualPosition3;
   float ActualPosition4;

   /* Outputs */

   float Scale;
   uint8_t Outputs[4];
   float CommandedPosition1;
   float CommandedPosition2;
   float CommandedPosition3;
   float CommandedPosition4;
   int32_t StepsPerMM1;
   int32_t StepsPerMM2;
   int32_t StepsPerMM3;
   int32_t StepsPerMM4;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
