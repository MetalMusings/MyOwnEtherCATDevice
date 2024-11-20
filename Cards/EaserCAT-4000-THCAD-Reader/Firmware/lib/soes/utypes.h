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

   /* Outputs */

   float Scale;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
