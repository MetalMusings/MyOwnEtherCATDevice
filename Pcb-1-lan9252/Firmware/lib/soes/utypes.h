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
   uint32_t DiffT;
   uint32_t IndexByte;
   uint32_t IndexStatus;

   /* Outputs */

   int32_t EncPosScale;
   uint32_t IndexLatchEnable;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
