#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

/* Object dictionary storage */

typedef struct
{
   /* Identity */

   uint32_t serial;

   /* Inputs */

   uint8_t Input[12];
   float Velocity;

   /* Outputs */

   uint8_t Output[4];

   /* Parameters */

   float VelocityScale;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
