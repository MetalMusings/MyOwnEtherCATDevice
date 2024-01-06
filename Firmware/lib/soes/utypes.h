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

   /* Outputs */

   int32_t EncPosScale;
   uint32_t IndexLatchEnable;
   struct
   {
      double CommandedPosition;
      double CommandedVelocity;
   } StepGenIn1;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
