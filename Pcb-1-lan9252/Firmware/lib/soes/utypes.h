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

   /* Outputs */

   int32_t EncPosScale;
   uint32_t EncIndexCEnable;

} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
