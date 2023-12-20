#ifndef __UTYPES_H__
#define __UTYPES_H__

#include "cc.h"

/* Object dictionary storage */

typedef struct
{
   /* Identity */

   uint32_t serial;

   /* Inputs */

   struct
   {
      int32_t ECount;
      uint8_t IndexTriggered;
   } EncoderOut;
   uint16_t ControlWord;
   uint8_t OpMode;
   int32_t TargetPosition;
   int32_t TargetVelocity;

   /* Outputs */

   uint16_t StatusWord;
   uint8_t OpModeDisplay;
   int32_t ActualPosition;
   int32_t ActualVelocity;
   int32_t ActualTorque;
   struct
   {
      uint8_t IndexEnable;
      uint8_t Reset;
   } EncoderIn;

   /* Parameters */

   struct
   {
      uint16_t Period;
      float Resolution;
   } StepperData;
} _Objects;

extern _Objects Obj;

#endif /* __UTYPES_H__ */
