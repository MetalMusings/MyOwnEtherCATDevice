
#ifndef __Stm32F4_Encoder_H__
#define __Stm32F4_Encoder_H__
#include <Arduino.h>


#define GPIO_Speed_50MHz    0x02  /*!< Fast speed */

//#define GPIO_MODER_MODER0                    ((uint32_t)0x00000003)
#define GPIO_Mode_OUT   0x01
//#define GPIO_OSPEEDER_OSPEEDR0               ((uint32_t)0x00000003)
//#define GPIO_OTYPER_OT_0                     ((uint32_t)0x00000001)
//#define GPIO_PUPDR_PUPDR0                    ((uint32_t)0x00000003)

#define GPIO_PuPd_NOPULL   0x00
#define GPIO_Mode_AF       0x02
#define GPIO_OType_PP      0x00
#define GPIO_PuPd_NOPULL   0x00
#define GPIO_Pin_0                 ((uint16_t)0x0001)  /* Pin 0 selected */
#define GPIO_Pin_1                 ((uint16_t)0x0002)  /* Pin 1 selected */
#define GPIO_Pin_2                 ((uint16_t)0x0004)  /* Pin 2 selected */
#define GPIO_Pin_3                 ((uint16_t)0x0008)  /* Pin 3 selected */
#define GPIO_Pin_4                 ((uint16_t)0x0010)  /* Pin 4 selected */
#define GPIO_Pin_5                 ((uint16_t)0x0020)  /* Pin 5 selected */
#define GPIO_Pin_6                 ((uint16_t)0x0040)  /* Pin 6 selected */
#define GPIO_Pin_7                 ((uint16_t)0x0080)  /* Pin 7 selected */
#define GPIO_Pin_8                 ((uint16_t)0x0100)  /* Pin 8 selected */
#define GPIO_Pin_9                 ((uint16_t)0x0200)  /* Pin 9 selected */
#define GPIO_Pin_10                ((uint16_t)0x0400)  /* Pin 10 selected */
#define GPIO_Pin_11                ((uint16_t)0x0800)  /* Pin 11 selected */
#define GPIO_Pin_12                ((uint16_t)0x1000)  /* Pin 12 selected */
#define GPIO_Pin_13                ((uint16_t)0x2000)  /* Pin 13 selected */
#define GPIO_Pin_14                ((uint16_t)0x4000)  /* Pin 14 selected */
#define GPIO_Pin_15                ((uint16_t)0x8000)  /* Pin 15 selected */
#define GPIO_Pin_All               ((uint16_t)0xFFFF)  /* All pins selected */




typedef struct TIM_TimeBaseInitTypeDef
{
  uint16_t TIM_Prescaler;         /*!< Specifies the prescaler value used to divide the TIM clock.
                                       This parameter can be a number between 0x0000 and 0xFFFF */

  uint16_t TIM_CounterMode;       /*!< Specifies the counter mode.
                                       This parameter can be a value of @ref TIM_Counter_Mode */

  uint32_t TIM_Period;            /*!< Specifies the period value to be loaded into the active
                                       Auto-Reload Register at the next update event.
                                       This parameter must be a number between 0x0000 and 0xFFFF.  */

  uint16_t TIM_ClockDivision;     /*!< Specifies the clock division.
                                      This parameter can be a value of @ref TIM_Clock_Division_CKD */

  uint8_t TIM_RepetitionCounter;  /*!< Specifies the repetition counter value. Each time the RCR downcounter
                                       reaches zero, an update event is generated and counting restarts
                                       from the RCR value (N).
                                       This means in PWM mode that (N+1) corresponds to:
                                          - the number of PWM periods in edge-aligned mode
                                          - the number of half PWM period in center-aligned mode
                                       This parameter must be a number between 0x00 and 0xFF.
                                       @note This parameter is valid only for TIM1 and TIM8. */
} TIM_TimeBaseInitTypeDef;


typedef struct
{
  uint16_t setcount;

} encoder;





typedef struct
{
  uint16_t TIM_OCMode;        /*!< Specifies the TIM mode.
                                   This parameter can be a value of @ref TIM_Output_Compare_and_PWM_modes */

  uint16_t TIM_OutputState;   /*!< Specifies the TIM Output Compare state.
                                   This parameter can be a value of @ref TIM_Output_Compare_State */

  uint16_t TIM_OutputNState;  /*!< Specifies the TIM complementary Output Compare state.
                                   This parameter can be a value of @ref TIM_Output_Compare_N_State
                                   @note This parameter is valid only for TIM1 and TIM8. */

  uint32_t TIM_Pulse;         /*!< Specifies the pulse value to be loaded into the Capture Compare Register.
                                   This parameter can be a number between 0x0000 and 0xFFFF */

  uint16_t TIM_OCPolarity;    /*!< Specifies the output polarity.
                                   This parameter can be a value of @ref TIM_Output_Compare_Polarity */

  uint16_t TIM_OCNPolarity;   /*!< Specifies the complementary output polarity.
                                   This parameter can be a value of @ref TIM_Output_Compare_N_Polarity
                                   @note This parameter is valid only for TIM1 and TIM8. */

  uint16_t TIM_OCIdleState;   /*!< Specifies the TIM Output Compare pin state during Idle state.
                                   This parameter can be a value of @ref TIM_Output_Compare_Idle_State
                                   @note This parameter is valid only for TIM1 and TIM8. */

  uint16_t TIM_OCNIdleState;  /*!< Specifies the TIM Output Compare pin state during Idle state.
                                   This parameter can be a value of @ref TIM_Output_Compare_N_Idle_State
                                   @note This parameter is valid only for TIM1 and TIM8. */
} TIM_OCInitTypeDef;



typedef struct
{

  uint16_t TIM_Channel;      /*!< Specifies the TIM channel.
                                  This parameter can be a value of @ref TIM_Channel */

  uint16_t TIM_ICPolarity;   /*!< Specifies the active edge of the input signal.
                                  This parameter can be a value of @ref TIM_Input_Capture_Polarity */

  uint16_t TIM_ICSelection;  /*!< Specifies the input.
                                  This parameter can be a value of @ref TIM_Input_Capture_Selection */

  uint16_t TIM_ICPrescaler;  /*!< Specifies the Input Capture Prescaler.
                                  This parameter can be a value of @ref TIM_Input_Capture_Prescaler */

  uint16_t TIM_ICFilter;     /*!< Specifies the input capture filter.
                                  This parameter can be a number between 0x0 and 0xF */
} TIM_ICInitTypeDef;



typedef struct
{

  uint16_t TIM_OSSRState;        /*!< Specifies the Off-State selection used in Run mode.
                                      This parameter can be a value of @ref TIM_OSSR_Off_State_Selection_for_Run_mode_state */

  uint16_t TIM_OSSIState;        /*!< Specifies the Off-State used in Idle state.
                                      This parameter can be a value of @ref TIM_OSSI_Off_State_Selection_for_Idle_mode_state */

  uint16_t TIM_LOCKLevel;        /*!< Specifies the LOCK level parameters.
                                      This parameter can be a value of @ref TIM_Lock_level */

  uint16_t TIM_DeadTime;         /*!< Specifies the delay time between the switching-off and the
                                      switching-on of the outputs.
                                      This parameter can be a number between 0x00 and 0xFF  */

  uint16_t TIM_Break;            /*!< Specifies whether the TIM Break input is enabled or not.
                                      This parameter can be a value of @ref TIM_Break_Input_enable_disable */

  uint16_t TIM_BreakPolarity;    /*!< Specifies the TIM Break Input pin polarity.
                                      This parameter can be a value of @ref TIM_Break_Polarity */

  uint16_t TIM_AutomaticOutput;  /*!< Specifies whether the TIM Automatic Output feature is enabled or not.
                                      This parameter can be a value of @ref TIM_AOE_Bit_Set_Reset */
} TIM_BDTRInitTypeDef;


#define GPIO_PinSource0            ((uint8_t)0x00)
#define GPIO_PinSource1            ((uint8_t)0x01)
#define GPIO_PinSource2            ((uint8_t)0x02)
#define GPIO_PinSource3            ((uint8_t)0x03)
#define GPIO_PinSource4            ((uint8_t)0x04)
#define GPIO_PinSource5            ((uint8_t)0x05)
#define GPIO_PinSource6            ((uint8_t)0x06)
#define GPIO_PinSource7            ((uint8_t)0x07)
#define GPIO_PinSource8            ((uint8_t)0x08)
#define GPIO_PinSource9            ((uint8_t)0x09)
#define GPIO_PinSource10           ((uint8_t)0x0A)
#define GPIO_PinSource11           ((uint8_t)0x0B)
#define GPIO_PinSource12           ((uint8_t)0x0C)
#define GPIO_PinSource13           ((uint8_t)0x0D)
#define GPIO_PinSource14           ((uint8_t)0x0E)
#define GPIO_PinSource15           ((uint8_t)0x0F)
#define GPIO_AF_TIM8          ((uint8_t)0x03)  /* TIM8 Alternate Function mapping */
#define GPIO_AF_TIM3          ((uint8_t)0x02)  /* TIM3 Alternate Function mapping */
#define GPIO_AF_TIM4          ((uint8_t)0x02)  /* TIM4 Alternate Function mapping */
#define GPIO_AF_TIM5          ((uint8_t)0x02)  /* TIM5 Alternate Function mapping */
#define GPIO_AF_TIM1          ((uint8_t)0x01)  /* TIM1 Alternate Function mapping */
#define GPIO_AF_TIM2          ((uint8_t)0x01)  /* TIM2 Alternate Function mapping */

#define TIM4                ((TIM_TypeDef *) TIM4_BASE)
#define TIM8                ((TIM_TypeDef *) TIM8_BASE)
#define TIM_EncoderMode_TI12               ((uint16_t)0x0003)
#define  TIM_ICPolarity_Rising             ((uint16_t)0x0000)
#define  TIM_ICPolarity_Falling            ((uint16_t)0x0002)
//#define  TIM_CR1_CEN                         ((uint16_t)0x0001)            /*!<Counter enable */
//#define  TIM_CR1_CEN                         ((uint16_t)0x0001)            /*!<Counter enable */

#define IS_TIM_ALL_PERIPH(PERIPH) (((PERIPH) == TIM1) || \
                                   ((PERIPH) == TIM2) || \
                                   ((PERIPH) == TIM3) || \
                                   ((PERIPH) == TIM4) || \
                                   ((PERIPH) == TIM5) || \
                                   ((PERIPH) == TIM6) || \
                                   ((PERIPH) == TIM7) || \
                                   ((PERIPH) == TIM8) || \
                                   ((PERIPH) == TIM9) || \
                                   ((PERIPH) == TIM10) || \
                                   ((PERIPH) == TIM11) || \
                                   ((PERIPH) == TIM12) || \
                                   (((PERIPH) == TIM13) || \
                                    ((PERIPH) == TIM14)))

#define IS_TIM_LIST1_PERIPH(PERIPH) (((PERIPH) == TIM1) || \
                                     ((PERIPH) == TIM2) || \
                                     ((PERIPH) == TIM3) || \
                                     ((PERIPH) == TIM4) || \
                                     ((PERIPH) == TIM5) || \
                                     ((PERIPH) == TIM8) || \
                                     ((PERIPH) == TIM9) || \
                                     ((PERIPH) == TIM10) || \
                                     ((PERIPH) == TIM11) || \
                                     ((PERIPH) == TIM12) || \
                                     ((PERIPH) == TIM13) || \
                                     ((PERIPH) == TIM14))


#define IS_TIM_LIST2_PERIPH(PERIPH) (((PERIPH) == TIM1) || \
                                     ((PERIPH) == TIM2) || \
                                     ((PERIPH) == TIM3) || \
                                     ((PERIPH) == TIM4) || \
                                     ((PERIPH) == TIM5) || \
                                     ((PERIPH) == TIM8) || \
                                     ((PERIPH) == TIM9) || \
                                     ((PERIPH) == TIM12))

#define IS_TIM_LIST3_PERIPH(PERIPH) (((PERIPH) == TIM1) || \
                                     ((PERIPH) == TIM2) || \
                                     ((PERIPH) == TIM3) || \
                                     ((PERIPH) == TIM4) || \
                                     ((PERIPH) == TIM5) || \
                                     ((PERIPH) == TIM8))

#define IS_TIM_LIST4_PERIPH(PERIPH) (((PERIPH) == TIM1) || \
                                     ((PERIPH) == TIM8))

#define IS_TIM_LIST5_PERIPH(PERIPH) (((PERIPH) == TIM1) || \
                                     ((PERIPH) == TIM2) || \
                                     ((PERIPH) == TIM3) || \
                                     ((PERIPH) == TIM4) || \
                                     ((PERIPH) == TIM5) || \
                                     ((PERIPH) == TIM6) || \
                                     ((PERIPH) == TIM7) || \
                                     ((PERIPH) == TIM8))

#define IS_TIM_LIST6_PERIPH(TIMx)(((TIMx) == TIM2) || \
                                  ((TIMx) == TIM5) || \
                                  ((TIMx) == TIM11))



#define TIM_OCMode_Timing                  ((uint16_t)0x0000)
#define TIM_OCMode_Active                  ((uint16_t)0x0010)
#define TIM_OCMode_Inactive                ((uint16_t)0x0020)
#define TIM_OCMode_Toggle                  ((uint16_t)0x0030)
#define TIM_OCMode_PWM1                    ((uint16_t)0x0060)
#define TIM_OCMode_PWM2                    ((uint16_t)0x0070)
//#define IS_TIM_OC_MODE(MODE) (((MODE) == TIM_OCMode_Timing) || \
//                              ((MODE) == TIM_OCMode_Active) || \
//                              ((MODE) == TIM_OCMode_Inactive) || \
//                              ((MODE) == TIM_OCMode_Toggle)|| \
//                              ((MODE) == TIM_OCMode_PWM1) || \
//                              ((MODE) == TIM_OCMode_PWM2))
#define IS_TIM_OCM(MODE) (((MODE) == TIM_OCMode_Timing) || \
                          ((MODE) == TIM_OCMode_Active) || \
                          ((MODE) == TIM_OCMode_Inactive) || \
                          ((MODE) == TIM_OCMode_Toggle)|| \
                          ((MODE) == TIM_OCMode_PWM1) || \
                          ((MODE) == TIM_OCMode_PWM2) ||  \
                          ((MODE) == TIM_ForcedAction_Active) || \
                          ((MODE) == TIM_ForcedAction_InActive))


#define TIM_OPMode_Single                  ((uint16_t)0x0008)
#define TIM_OPMode_Repetitive              ((uint16_t)0x0000)
//#define IS_TIM_OPM_MODE(MODE) (((MODE) == TIM_OPMode_Single) || \
//                               ((MODE) == TIM_OPMode_Repetitive))


#define TIM_Channel_1                      ((uint16_t)0x0000)
#define TIM_Channel_2                      ((uint16_t)0x0004)
#define TIM_Channel_3                      ((uint16_t)0x0008)
#define TIM_Channel_4                      ((uint16_t)0x000C)

#define IS_TIM_CHANNEL(CHANNEL) (((CHANNEL) == TIM_Channel_1) || \
                                 ((CHANNEL) == TIM_Channel_2) || \
                                 ((CHANNEL) == TIM_Channel_3) || \
                                 ((CHANNEL) == TIM_Channel_4))

#define IS_TIM_PWMI_CHANNEL(CHANNEL) (((CHANNEL) == TIM_Channel_1) || \
                                      ((CHANNEL) == TIM_Channel_2))
#define IS_TIM_COMPLEMENTARY_CHANNEL(CHANNEL) (((CHANNEL) == TIM_Channel_1) || \
    ((CHANNEL) == TIM_Channel_2) || \
    ((CHANNEL) == TIM_Channel_3))


#define TIM_CKD_DIV1                       ((uint16_t)0x0000)
#define TIM_CKD_DIV2                       ((uint16_t)0x0100)
#define TIM_CKD_DIV4                       ((uint16_t)0x0200)
#define IS_TIM_CKD_DIV(DIV) (((DIV) == TIM_CKD_DIV1) || \
                             ((DIV) == TIM_CKD_DIV2) || \
                             ((DIV) == TIM_CKD_DIV4))


#define TIM_CounterMode_Up                 ((uint16_t)0x0000)
#define TIM_CounterMode_Down               ((uint16_t)0x0010)
#define TIM_CounterMode_CenterAligned1     ((uint16_t)0x0020)
#define TIM_CounterMode_CenterAligned2     ((uint16_t)0x0040)
#define TIM_CounterMode_CenterAligned3     ((uint16_t)0x0060)
//#define IS_TIM_COUNTER_MODE(MODE) (((MODE) == TIM_CounterMode_Up) ||  \
//                                   ((MODE) == TIM_CounterMode_Down) || \
//                                   ((MODE) == TIM_CounterMode_CenterAligned1) || \
//                                   ((MODE) == TIM_CounterMode_CenterAligned2) || \
//                                   ((MODE) == TIM_CounterMode_CenterAligned3))


#define TIM_OCPolarity_High                ((uint16_t)0x0000)
#define TIM_OCPolarity_Low                 ((uint16_t)0x0002)
//#define IS_TIM_OC_POLARITY(POLARITY) (((POLARITY) == TIM_OCPolarity_High) || \
//                                      ((POLARITY) == TIM_OCPolarity_Low))


#define TIM_OCNPolarity_High               ((uint16_t)0x0000)
#define TIM_OCNPolarity_Low                ((uint16_t)0x0008)
//#define IS_TIM_OCN_POLARITY(POLARITY) (((POLARITY) == TIM_OCNPolarity_High) || \
//                                       ((POLARITY) == TIM_OCNPolarity_Low))

#define TIM_OutputState_Disable            ((uint16_t)0x0000)
#define TIM_OutputState_Enable             ((uint16_t)0x0001)
#define IS_TIM_OUTPUT_STATE(STATE) (((STATE) == TIM_OutputState_Disable) || \
                                    ((STATE) == TIM_OutputState_Enable))


#define TIM_OutputNState_Disable           ((uint16_t)0x0000)
#define TIM_OutputNState_Enable            ((uint16_t)0x0004)
#define IS_TIM_OUTPUTN_STATE(STATE) (((STATE) == TIM_OutputNState_Disable) || \
                                     ((STATE) == TIM_OutputNState_Enable))


#define TIM_CCx_Enable                      ((uint16_t)0x0001)
#define TIM_CCx_Disable                     ((uint16_t)0x0000)
#define IS_TIM_CCX(CCX) (((CCX) == TIM_CCx_Enable) || \
                         ((CCX) == TIM_CCx_Disable))


#define TIM_CCxN_Enable                     ((uint16_t)0x0004)
#define TIM_CCxN_Disable                    ((uint16_t)0x0000)
#define IS_TIM_CCXN(CCXN) (((CCXN) == TIM_CCxN_Enable) || \
                           ((CCXN) == TIM_CCxN_Disable))


#define TIM_Break_Enable                   ((uint16_t)0x1000)
#define TIM_Break_Disable                  ((uint16_t)0x0000)
//#define IS_TIM_BREAK_STATE(STATE) (((STATE) == TIM_Break_Enable) || \
//                                   ((STATE) == TIM_Break_Disable))


#define TIM_BreakPolarity_Low              ((uint16_t)0x0000)
#define TIM_BreakPolarity_High             ((uint16_t)0x2000)
//#define IS_TIM_BREAK_POLARITY(POLARITY) (((POLARITY) == TIM_BreakPolarity_Low) || \
//    ((POLARITY) == TIM_BreakPolarity_High))


#define TIM_AutomaticOutput_Enable         ((uint16_t)0x4000)
#define TIM_AutomaticOutput_Disable        ((uint16_t)0x0000)
//#define IS_TIM_AUTOMATIC_OUTPUT_STATE(STATE) (((STATE) == TIM_AutomaticOutput_Enable) || \
//    ((STATE) == TIM_AutomaticOutput_Disable))


#define TIM_LOCKLevel_OFF                  ((uint16_t)0x0000)
#define TIM_LOCKLevel_1                    ((uint16_t)0x0100)
#define TIM_LOCKLevel_2                    ((uint16_t)0x0200)
#define TIM_LOCKLevel_3                    ((uint16_t)0x0300)
//#define IS_TIM_LOCK_LEVEL(LEVEL) (((LEVEL) == TIM_LOCKLevel_OFF) || \
//                                  ((LEVEL) == TIM_LOCKLevel_1) || \
//                                  ((LEVEL) == TIM_LOCKLevel_2) || \
//                                  ((LEVEL) == TIM_LOCKLevel_3))


#define TIM_OSSIState_Enable               ((uint16_t)0x0400)
#define TIM_OSSIState_Disable              ((uint16_t)0x0000)
//#define IS_TIM_OSSI_STATE(STATE) (((STATE) == TIM_OSSIState_Enable) || \
//                                  ((STATE) == TIM_OSSIState_Disable))


#define TIM_OSSRState_Enable               ((uint16_t)0x0800)
#define TIM_OSSRState_Disable              ((uint16_t)0x0000)
//#define IS_TIM_OSSR_STATE(STATE) (((STATE) == TIM_OSSRState_Enable) || \
//                                  ((STATE) == TIM_OSSRState_Disable))


#define TIM_OCIdleState_Set                ((uint16_t)0x0100)
#define TIM_OCIdleState_Reset              ((uint16_t)0x0000)
//#define IS_TIM_OCIDLE_STATE(STATE) (((STATE) == TIM_OCIdleState_Set) || \
//                                    ((STATE) == TIM_OCIdleState_Reset))
//

#define TIM_OCNIdleState_Set               ((uint16_t)0x0200)
#define TIM_OCNIdleState_Reset             ((uint16_t)0x0000)
//#define IS_TIM_OCNIDLE_STATE(STATE) (((STATE) == TIM_OCNIdleState_Set) || \
//                                     ((STATE) == TIM_OCNIdleState_Reset))


#define  TIM_ICPolarity_Rising             ((uint16_t)0x0000)
#define  TIM_ICPolarity_Falling            ((uint16_t)0x0002)
#define  TIM_ICPolarity_BothEdge           ((uint16_t)0x000A)
//#define IS_TIM_IC_POLARITY(POLARITY) (((POLARITY) == TIM_ICPolarity_Rising) || \
//                                      ((POLARITY) == TIM_ICPolarity_Falling)|| \
//                                      ((POLARITY) == TIM_ICPolarity_BothEdge))


#define TIM_ICSelection_DirectTI           ((uint16_t)0x0001) /*!< TIM Input 1, 2, 3 or 4 is selected to be 
                                                                   connected to IC1, IC2, IC3 or IC4, respectively */
#define TIM_ICSelection_IndirectTI         ((uint16_t)0x0002) /*!< TIM Input 1, 2, 3 or 4 is selected to be
                                                                   connected to IC2, IC1, IC4 or IC3, respectively. */
#define TIM_ICSelection_TRC                ((uint16_t)0x0003) /*!< TIM Input 1, 2, 3 or 4 is selected to be connected to TRC. */
//#define IS_TIM_IC_SELECTION(SELECTION) (((SELECTION) == TIM_ICSelection_DirectTI) || \
//                                        ((SELECTION) == TIM_ICSelection_IndirectTI) || \
//                                        ((SELECTION) == TIM_ICSelection_TRC))


//#define TIM_ICPSC_DIV1                     ((uint16_t)0x0000) /*!< Capture performed each time an edge is detected on the capture input. */
//#define TIM_ICPSC_DIV2                     ((uint16_t)0x0004) /*!< Capture performed once every 2 events. */
//#define TIM_ICPSC_DIV4                     ((uint16_t)0x0008) /*!< Capture performed once every 4 events. */
//#define TIM_ICPSC_DIV8                     ((uint16_t)0x000C) /*!< Capture performed once every 8 events. */
//#define IS_TIM_IC_PRESCALER(PRESCALER) (((PRESCALER) == TIM_ICPSC_DIV1) || \
//                                        ((PRESCALER) == TIM_ICPSC_DIV2) || \
//                                        ((PRESCALER) == TIM_ICPSC_DIV4) || \
//                                        ((PRESCALER) == TIM_ICPSC_DIV8))


#define TIM_IT_Update                      ((uint16_t)0x0001)
//#define TIM_IT_CC1                         ((uint16_t)0x0002)
//#define TIM_IT_CC2                         ((uint16_t)0x0004)
//#define TIM_IT_CC3                         ((uint16_t)0x0008)
//#define TIM_IT_CC4                         ((uint16_t)0x0010)
//#define TIM_IT_COM                         ((uint16_t)0x0020)
#define TIM_IT_Trigger                     ((uint16_t)0x0040)
#define TIM_IT_Break                       ((uint16_t)0x0080)
#define IS_TIM_IT(IT) ((((IT) & (uint16_t)0xFF00) == 0x0000) && ((IT) != 0x0000))

#define IS_TIM_GET_IT(IT) (((IT) == TIM_IT_Update) || \
                           ((IT) == TIM_IT_CC1) || \
                           ((IT) == TIM_IT_CC2) || \
                           ((IT) == TIM_IT_CC3) || \
                           ((IT) == TIM_IT_CC4) || \
                           ((IT) == TIM_IT_COM) || \
                           ((IT) == TIM_IT_Trigger) || \
                           ((IT) == TIM_IT_Break))


//#define TIM_DMABase_CR1                    ((uint16_t)0x0000)
//#define TIM_DMABase_CR2                    ((uint16_t)0x0001)
//#define TIM_DMABase_SMCR                   ((uint16_t)0x0002)
//#define TIM_DMABase_DIER                   ((uint16_t)0x0003)
//#define TIM_DMABase_SR                     ((uint16_t)0x0004)
//#define TIM_DMABase_EGR                    ((uint16_t)0x0005)
//#define TIM_DMABase_CCMR1                  ((uint16_t)0x0006)
//#define TIM_DMABase_CCMR2                  ((uint16_t)0x0007)
//#define TIM_DMABase_CCER                   ((uint16_t)0x0008)
//#define TIM_DMABase_CNT                    ((uint16_t)0x0009)
//#define TIM_DMABase_PSC                    ((uint16_t)0x000A)
//#define TIM_DMABase_ARR                    ((uint16_t)0x000B)
//#define TIM_DMABase_RCR                    ((uint16_t)0x000C)
//#define TIM_DMABase_CCR1                   ((uint16_t)0x000D)
//#define TIM_DMABase_CCR2                   ((uint16_t)0x000E)
//#define TIM_DMABase_CCR3                   ((uint16_t)0x000F)
//#define TIM_DMABase_CCR4                   ((uint16_t)0x0010)
//#define TIM_DMABase_BDTR                   ((uint16_t)0x0011)
//#define TIM_DMABase_DCR                    ((uint16_t)0x0012)
//#define TIM_DMABase_OR                     ((uint16_t)0x0013)
//#define IS_TIM_DMA_BASE(BASE) (((BASE) == TIM_DMABase_CR1) || \
//                               ((BASE) == TIM_DMABase_CR2) || \
//                               ((BASE) == TIM_DMABase_SMCR) || \
//                               ((BASE) == TIM_DMABase_DIER) || \
//                               ((BASE) == TIM_DMABase_SR) || \
//                               ((BASE) == TIM_DMABase_EGR) || \
//                               ((BASE) == TIM_DMABase_CCMR1) || \
//                               ((BASE) == TIM_DMABase_CCMR2) || \
//                               ((BASE) == TIM_DMABase_CCER) || \
//                               ((BASE) == TIM_DMABase_CNT) || \
//                               ((BASE) == TIM_DMABase_PSC) || \
//                               ((BASE) == TIM_DMABase_ARR) || \
//                               ((BASE) == TIM_DMABase_RCR) || \
//                               ((BASE) == TIM_DMABase_CCR1) || \
//                               ((BASE) == TIM_DMABase_CCR2) || \
//                               ((BASE) == TIM_DMABase_CCR3) || \
//                               ((BASE) == TIM_DMABase_CCR4) || \
//                               ((BASE) == TIM_DMABase_BDTR) || \
//                               ((BASE) == TIM_DMABase_DCR) || \
//                               ((BASE) == TIM_DMABase_OR))


//#define TIM_DMABurstLength_1Transfer           ((uint16_t)0x0000)
//#define TIM_DMABurstLength_2Transfers          ((uint16_t)0x0100)
//#define TIM_DMABurstLength_3Transfers          ((uint16_t)0x0200)
//#define TIM_DMABurstLength_4Transfers          ((uint16_t)0x0300)
//#define TIM_DMABurstLength_5Transfers          ((uint16_t)0x0400)
//#define TIM_DMABurstLength_6Transfers          ((uint16_t)0x0500)
//#define TIM_DMABurstLength_7Transfers          ((uint16_t)0x0600)
//#define TIM_DMABurstLength_8Transfers          ((uint16_t)0x0700)
//#define TIM_DMABurstLength_9Transfers          ((uint16_t)0x0800)
//#define TIM_DMABurstLength_10Transfers         ((uint16_t)0x0900)
//#define TIM_DMABurstLength_11Transfers         ((uint16_t)0x0A00)
//#define TIM_DMABurstLength_12Transfers         ((uint16_t)0x0B00)
//#define TIM_DMABurstLength_13Transfers         ((uint16_t)0x0C00)
//#define TIM_DMABurstLength_14Transfers         ((uint16_t)0x0D00)
//#define TIM_DMABurstLength_15Transfers         ((uint16_t)0x0E00)
//#define TIM_DMABurstLength_16Transfers         ((uint16_t)0x0F00)
//#define TIM_DMABurstLength_17Transfers         ((uint16_t)0x1000)
//#define TIM_DMABurstLength_18Transfers         ((uint16_t)0x1100)
//#define IS_TIM_DMA_LENGTH(LENGTH) (((LENGTH) == TIM_DMABurstLength_1Transfer) || \
//                                   ((LENGTH) == TIM_DMABurstLength_2Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_3Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_4Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_5Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_6Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_7Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_8Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_9Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_10Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_11Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_12Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_13Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_14Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_15Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_16Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_17Transfers) || \
//                                   ((LENGTH) == TIM_DMABurstLength_18Transfers))


//#define TIM_DMA_Update                     ((uint16_t)0x0100)
//#define TIM_DMA_CC1                        ((uint16_t)0x0200)
//#define TIM_DMA_CC2                        ((uint16_t)0x0400)
//#define TIM_DMA_CC3                        ((uint16_t)0x0800)
//#define TIM_DMA_CC4                        ((uint16_t)0x1000)
//#define TIM_DMA_COM                        ((uint16_t)0x2000)
//#define TIM_DMA_Trigger                    ((uint16_t)0x4000)
//#define IS_TIM_DMA_SOURCE(SOURCE) ((((SOURCE) & (uint16_t)0x80FF) == 0x0000) && ((SOURCE) != 0x0000))



#define TIM_ExtTRGPSC_OFF                  ((uint16_t)0x0000)
#define TIM_ExtTRGPSC_DIV2                 ((uint16_t)0x1000)
#define TIM_ExtTRGPSC_DIV4                 ((uint16_t)0x2000)
#define TIM_ExtTRGPSC_DIV8                 ((uint16_t)0x3000)
#define IS_TIM_EXT_PRESCALER(PRESCALER) (((PRESCALER) == TIM_ExtTRGPSC_OFF) || \
    ((PRESCALER) == TIM_ExtTRGPSC_DIV2) || \
    ((PRESCALER) == TIM_ExtTRGPSC_DIV4) || \
    ((PRESCALER) == TIM_ExtTRGPSC_DIV8))


//#define TIM_TS_ITR0                        ((uint16_t)0x0000)
//#define TIM_TS_ITR1                        ((uint16_t)0x0010)
//#define TIM_TS_ITR2                        ((uint16_t)0x0020)
//#define TIM_TS_ITR3                        ((uint16_t)0x0030)
//#define TIM_TS_TI1F_ED                     ((uint16_t)0x0040)
//#define TIM_TS_TI1FP1                      ((uint16_t)0x0050)
//#define TIM_TS_TI2FP2                      ((uint16_t)0x0060)
//#define TIM_TS_ETRF                        ((uint16_t)0x0070)
//#define IS_TIM_TRIGGER_SELECTION(SELECTION) (((SELECTION) == TIM_TS_ITR0) || \
//    ((SELECTION) == TIM_TS_ITR1) || \
//    ((SELECTION) == TIM_TS_ITR2) || \
//    ((SELECTION) == TIM_TS_ITR3) || \
//    ((SELECTION) == TIM_TS_TI1F_ED) || \
//    ((SELECTION) == TIM_TS_TI1FP1) || \
//    ((SELECTION) == TIM_TS_TI2FP2) || \
//    ((SELECTION) == TIM_TS_ETRF))
#define IS_TIM_INTERNAL_TRIGGER_SELECTION(SELECTION) (((SELECTION) == TIM_TS_ITR0) || \
    ((SELECTION) == TIM_TS_ITR1) || \
    ((SELECTION) == TIM_TS_ITR2) || \
    ((SELECTION) == TIM_TS_ITR3))


#define TIM_TIxExternalCLK1Source_TI1      ((uint16_t)0x0050)
#define TIM_TIxExternalCLK1Source_TI2      ((uint16_t)0x0060)
#define TIM_TIxExternalCLK1Source_TI1ED    ((uint16_t)0x0040)


#define TIM_ExtTRGPolarity_Inverted        ((uint16_t)0x8000)
#define TIM_ExtTRGPolarity_NonInverted     ((uint16_t)0x0000)
#define IS_TIM_EXT_POLARITY(POLARITY) (((POLARITY) == TIM_ExtTRGPolarity_Inverted) || \
                                       ((POLARITY) == TIM_ExtTRGPolarity_NonInverted))


#define TIM_PSCReloadMode_Update           ((uint16_t)0x0000)
#define TIM_PSCReloadMode_Immediate        ((uint16_t)0x0001)
#define IS_TIM_PRESCALER_RELOAD(RELOAD) (((RELOAD) == TIM_PSCReloadMode_Update) || \
    ((RELOAD) == TIM_PSCReloadMode_Immediate))

#define TIM_ForcedAction_Active            ((uint16_t)0x0050)
#define TIM_ForcedAction_InActive          ((uint16_t)0x0040)
#define IS_TIM_FORCED_ACTION(ACTION) (((ACTION) == TIM_ForcedAction_Active) || \
                                      ((ACTION) == TIM_ForcedAction_InActive))


#define TIM_EncoderMode_TI1                ((uint16_t)0x0001)
#define TIM_EncoderMode_TI2                ((uint16_t)0x0002)
#define TIM_EncoderMode_TI12               ((uint16_t)0x0003)
//#define IS_TIM_ENCODER_MODE(MODE) (((MODE) == TIM_EncoderMode_TI1) || \
//                                   ((MODE) == TIM_EncoderMode_TI2) || \
//                                   ((MODE) == TIM_EncoderMode_TI12))


//#define TIM_EventSource_Update             ((uint16_t)0x0001)
//#define TIM_EventSource_CC1                ((uint16_t)0x0002)
//#define TIM_EventSource_CC2                ((uint16_t)0x0004)
//#define TIM_EventSource_CC3                ((uint16_t)0x0008)
//#define TIM_EventSource_CC4                ((uint16_t)0x0010)
//#define TIM_EventSource_COM                ((uint16_t)0x0020)
//#define TIM_EventSource_Trigger            ((uint16_t)0x0040)
//#define TIM_EventSource_Break              ((uint16_t)0x0080)
//#define IS_TIM_EVENT_SOURCE(SOURCE) ((((SOURCE) & (uint16_t)0xFF00) == 0x0000) && ((SOURCE) != 0x0000))



#define TIM_UpdateSource_Global            ((uint16_t)0x0000) /*!< Source of update is the counter overflow/underflow
                                                                   or the setting of UG bit, or an update generation
                                                                   through the slave mode controller. */
#define TIM_UpdateSource_Regular           ((uint16_t)0x0001) /*!< Source of update is counter overflow/underflow. */
#define IS_TIM_UPDATE_SOURCE(SOURCE) (((SOURCE) == TIM_UpdateSource_Global) || \
                                      ((SOURCE) == TIM_UpdateSource_Regular))


#define TIM_OCPreload_Enable               ((uint16_t)0x0008)
#define TIM_OCPreload_Disable              ((uint16_t)0x0000)
#define IS_TIM_OCPRELOAD_STATE(STATE) (((STATE) == TIM_OCPreload_Enable) || \
                                       ((STATE) == TIM_OCPreload_Disable))


#define TIM_OCFast_Enable                  ((uint16_t)0x0004)
#define TIM_OCFast_Disable                 ((uint16_t)0x0000)
#define IS_TIM_OCFAST_STATE(STATE) (((STATE) == TIM_OCFast_Enable) || \
                                    ((STATE) == TIM_OCFast_Disable))


#define TIM_OCClear_Enable                 ((uint16_t)0x0080)
#define TIM_OCClear_Disable                ((uint16_t)0x0000)
#define IS_TIM_OCCLEAR_STATE(STATE) (((STATE) == TIM_OCClear_Enable) || \
                                     ((STATE) == TIM_OCClear_Disable))

#define TIM_TRGOSource_Reset               ((uint16_t)0x0000)
#define TIM_TRGOSource_Enable              ((uint16_t)0x0010)
#define TIM_TRGOSource_Update              ((uint16_t)0x0020)
#define TIM_TRGOSource_OC1                 ((uint16_t)0x0030)
#define TIM_TRGOSource_OC1Ref              ((uint16_t)0x0040)
#define TIM_TRGOSource_OC2Ref              ((uint16_t)0x0050)
#define TIM_TRGOSource_OC3Ref              ((uint16_t)0x0060)
#define TIM_TRGOSource_OC4Ref              ((uint16_t)0x0070)
//#define IS_TIM_TRGO_SOURCE(SOURCE) (((SOURCE) == TIM_TRGOSource_Reset) || \
//                                    ((SOURCE) == TIM_TRGOSource_Enable) || \
//                                    ((SOURCE) == TIM_TRGOSource_Update) || \
//                                    ((SOURCE) == TIM_TRGOSource_OC1) || \
//                                    ((SOURCE) == TIM_TRGOSource_OC1Ref) || \
//                                    ((SOURCE) == TIM_TRGOSource_OC2Ref) || \
//                                    ((SOURCE) == TIM_TRGOSource_OC3Ref) || \
//                                    ((SOURCE) == TIM_TRGOSource_OC4Ref))




#define TIM_SlaveMode_Reset                ((uint16_t)0x0004)
#define TIM_SlaveMode_Gated                ((uint16_t)0x0005)
#define TIM_SlaveMode_Trigger              ((uint16_t)0x0006)
#define TIM_SlaveMode_External1            ((uint16_t)0x0007)
//#define IS_TIM_SLAVE_MODE(MODE) (((MODE) == TIM_SlaveMode_Reset) || \
//                                 ((MODE) == TIM_SlaveMode_Gated) || \
//                                 ((MODE) == TIM_SlaveMode_Trigger) || \
//                                 ((MODE) == TIM_SlaveMode_External1))

#define TIM_MasterSlaveMode_Enable         ((uint16_t)0x0080)
#define TIM_MasterSlaveMode_Disable        ((uint16_t)0x0000)
//#define IS_TIM_MSM_STATE(STATE) (((STATE) == TIM_MasterSlaveMode_Enable) || \
//                                 ((STATE) == TIM_MasterSlaveMode_Disable))

#define TIM2_TIM8_TRGO                     ((uint16_t)0x0000)
#define TIM2_ETH_PTP                       ((uint16_t)0x0400)
#define TIM2_USBFS_SOF                     ((uint16_t)0x0800)
#define TIM2_USBHS_SOF                     ((uint16_t)0x0C00)

#define TIM5_GPIO                          ((uint16_t)0x0000)
#define TIM5_LSI                           ((uint16_t)0x0040)
#define TIM5_LSE                           ((uint16_t)0x0080)
#define TIM5_RTC                           ((uint16_t)0x00C0)

#define TIM11_GPIO                         ((uint16_t)0x0000)
#define TIM11_HSE                          ((uint16_t)0x0002)


enum EncTimer {
  Tim2,
  Tim3,
  Tim4,
  Tim8
};

class Encoder
{
  private:
    int _pin;

  public:
    void  eattach(int enco);
    Encoder();
    void SetCount( enum EncTimer enc, int64_t Counter);
    uint16_t  GetCount( enum EncTimer enc);
    void attachh(int encoNumber);
    uint16_t c;
};

void rcc_config();
void GpioConfigPortA(GPIO_TypeDef * GPIOx) ;
void GpioConfigPortC(GPIO_TypeDef * GPIOx) ;
void GpioConfigPortD(GPIO_TypeDef * GPIOx) ;
void TIM_EncoderInterConfig(TIM_TypeDef* TIMx, uint16_t TIM_EncoderMode, uint16_t TIM_IC1Polarity, uint16_t TIM_IC2Polarity);
void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef* TIM_TimeBaseInitStruct);
void TIM_TimeBaseInit(TIM_TypeDef* TIMx, TIM_TimeBaseInitTypeDef* TIM_TimeBaseInitStruct);
#endif
