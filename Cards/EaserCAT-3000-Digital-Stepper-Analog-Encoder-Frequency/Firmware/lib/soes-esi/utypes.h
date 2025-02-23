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
   float Frequency;
   uint8_t Input8;
   float ActualPosition1;
   float ActualPosition2;
   float ActualPosition3;
   float ActualPosition4;

   /* Outputs */

   float Scale;
   uint8_t Output4;
   float CommandedPosition1;
   float CommandedPosition2;
   float CommandedPosition3;
   float CommandedPosition4;
   float StepsPerMM1;
   float StepsPerMM2;
   float StepsPerMM3;
   float StepsPerMM4;
   float MaxAcceleration1;
   float MaxAcceleration2;
   float MaxAcceleration3;
   float MaxAcceleration4;
   uint8_t Enable1;
   uint8_t Enable2;
   uint8_t Enable3;
   uint8_t Enable4;

   /* Parameters */

   uint32_t BasePeriod;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
