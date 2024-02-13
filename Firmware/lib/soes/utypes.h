#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

/* Object dictionary storage */

typedef struct
{
   /* Identity */

   uint32_t serial;

   /* Inputs */

   double EncPos;
   double EncFrequency;
   uint32_t DiffT;
   uint32_t IndexByte;
   uint32_t IndexStatus;
   struct
   {
      double ActualPosition;
   } StepGenOut1;
   struct
   {
      double ActualPosition;
   } StepGenOut2;
   int32_t D1;
   int32_t D2;
   int32_t D3;
   int32_t D4;

   /* Outputs */

   int32_t EncPosScale;
   uint32_t IndexLatchEnable;
   struct
   {
      double CommandedPosition;
      int16_t StepsPerMM;
   } StepGenIn1;
   struct
   {
      double CommandedPosition;
      int16_t StepsPerMM;
   } StepGenIn2;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
