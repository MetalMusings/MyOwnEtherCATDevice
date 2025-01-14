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
   float Velocity;
   uint32_t Frequency;

   /* Outputs */

   uint8_t Output4;

   /* Parameters */

   float VelocityScale;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
