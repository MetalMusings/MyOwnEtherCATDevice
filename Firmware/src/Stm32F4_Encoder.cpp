#include <Stm32F4_Encoder.h>

/*
   Stm32F4_Encoder.cpp
    Created on: Nov 20, 2020
        Author: GoktugH.
*/
// TIM2, TIM3, TIM4, TIM8
Encoder::Encoder()
{
    int unit;
}

// void Encoder::SetCount(enum EncTimer enc, int64_t Counter)
void Encoder::SetCount(int64_t Counter)
{
    tim_base->CNT = Counter;
}
// uint16_t Encoder::GetCount(enum EncTimer enc)
uint16_t Encoder::GetCount()
{
    return tim_base->CNT;
}

void GpioConfigPortA(GPIO_TypeDef *GPIOx)
{

    uint32_t pinpos = 0x00, pos = 0x00, currentpin = 0x00;

    /* ------------------------- Configure the port pins ---------------- */
    /*-- GPIO Mode Configuration --*/
    for (pinpos = 0x00; pinpos < 0x10; pinpos++)
    {
        pos = ((uint32_t)0x01) << pinpos;
        /* Get the port pins position */
        currentpin = (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_6 | GPIO_Pin_7) & pos;

        if (currentpin == pos)
        {
            GPIOx->MODER &= ~(GPIO_MODER_MODER0 << (pinpos * 2));
            GPIOx->MODER |= (((uint32_t)GPIO_Mode_AF) << (pinpos * 2));

            if ((GPIO_Mode_AF == GPIO_Mode_OUT) || (GPIO_Mode_AF == GPIO_Mode_AF))
            {
                /* Check Speed mode parameters */

                /* Speed mode configuration */
                GPIOx->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR0 << (pinpos * 2));
                GPIOx->OSPEEDR |= ((uint32_t)(GPIO_Speed_50MHz) << (pinpos * 2));

                /* Check Output mode parameters */

                /* Output mode configuration*/
                GPIOx->OTYPER &= ~((GPIO_OTYPER_OT_0) << ((uint16_t)pinpos));
                GPIOx->OTYPER |= (uint16_t)(((uint16_t)GPIO_OType_PP) << ((uint16_t)pinpos));
            }

            /* Pull-up Pull down resistor configuration*/
            GPIOx->PUPDR &= ~(GPIO_PUPDR_PUPDR0 << ((uint16_t)pinpos * 2));
            GPIOx->PUPDR |= (((uint32_t)GPIO_PuPd_NOPULL) << (pinpos * 2));
        }
    }
}

void GpioConfigPortC(GPIO_TypeDef *GPIOx)
{

    uint32_t pinpos = 0x00, pos = 0x00, currentpin = 0x00;

    /* ------------------------- Configure the port pins ---------------- */
    /*-- GPIO Mode Configuration --*/
    for (pinpos = 0x00; pinpos < 0x10; pinpos++)
    {
        pos = ((uint32_t)0x01) << pinpos;
        /* Get the port pins position */
        currentpin = (GPIO_Pin_6 | GPIO_Pin_7) & pos;

        if (currentpin == pos)
        {
            GPIOx->MODER &= ~(GPIO_MODER_MODER0 << (pinpos * 2));
            GPIOx->MODER |= (((uint32_t)GPIO_Mode_AF) << (pinpos * 2));

            if ((GPIO_Mode_AF == GPIO_Mode_OUT) || (GPIO_Mode_AF == GPIO_Mode_AF))
            {
                /* Check Speed mode parameters */

                /* Speed mode configuration */
                GPIOx->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR0 << (pinpos * 2));
                GPIOx->OSPEEDR |= ((uint32_t)(GPIO_Speed_50MHz) << (pinpos * 2));

                /* Check Output mode parameters */

                /* Output mode configuration*/
                GPIOx->OTYPER &= ~((GPIO_OTYPER_OT_0) << ((uint16_t)pinpos));
                GPIOx->OTYPER |= (uint16_t)(((uint16_t)GPIO_OType_PP) << ((uint16_t)pinpos));
            }

            /* Pull-up Pull down resistor configuration*/
            GPIOx->PUPDR &= ~(GPIO_PUPDR_PUPDR0 << ((uint16_t)pinpos * 2));
            GPIOx->PUPDR |= (((uint32_t)GPIO_PuPd_NOPULL) << (pinpos * 2));
        }
    }
}

void GpioConfigPortD(GPIO_TypeDef *GPIOx)
{

    uint32_t pinpos = 0x00, pos = 0x00, currentpin = 0x00;

    /* ------------------------- Configure the port pins ---------------- */
    /*-- GPIO Mode Configuration --*/
    for (pinpos = 0x00; pinpos < 0x10; pinpos++)
    {
        pos = ((uint32_t)0x01) << pinpos;
        /* Get the port pins position */
        currentpin = (GPIO_Pin_12 | GPIO_Pin_13) & pos;

        if (currentpin == pos)
        {
            GPIOx->MODER &= ~(GPIO_MODER_MODER0 << (pinpos * 2));
            GPIOx->MODER |= (((uint32_t)GPIO_Mode_AF) << (pinpos * 2));

            if ((GPIO_Mode_AF == GPIO_Mode_OUT) || (GPIO_Mode_AF == GPIO_Mode_AF))
            {
                /* Check Speed mode parameters */

                /* Speed mode configuration */
                GPIOx->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR0 << (pinpos * 2));
                GPIOx->OSPEEDR |= ((uint32_t)(GPIO_Speed_50MHz) << (pinpos * 2));

                /* Check Output mode parameters */

                /* Output mode configuration*/
                GPIOx->OTYPER &= ~((GPIO_OTYPER_OT_0) << ((uint16_t)pinpos));
                GPIOx->OTYPER |= (uint16_t)(((uint16_t)GPIO_OType_PP) << ((uint16_t)pinpos));
            }

            /* Pull-up Pull down resistor configuration*/
            GPIOx->PUPDR &= ~(GPIO_PUPDR_PUPDR0 << ((uint16_t)pinpos * 2));
            GPIOx->PUPDR |= (((uint32_t)GPIO_PuPd_NOPULL) << (pinpos * 2));
        }
    }
}

void TIM_EncoderInterConfig(TIM_TypeDef *TIMx, uint16_t TIM_EncoderMode, uint16_t TIM_IC1Polarity, uint16_t TIM_IC2Polarity)
{

    uint16_t tmpsmcr = 0;
    uint16_t tmpccmr1 = 0;
    uint16_t tmpccer = 0;

    /* Get the TIMx SMCR register value */
    tmpsmcr = TIMx->SMCR;

    /* Get the TIMx CCMR1 register value */
    tmpccmr1 = TIMx->CCMR1;

    /* Get the TIMx CCER register value */
    tmpccer = TIMx->CCER;

    /* Set the encoder Mode */
    tmpsmcr &= (uint16_t)~TIM_SMCR_SMS;
    tmpsmcr |= TIM_EncoderMode;

    /* Select the Capture Compare 1 and the Capture Compare 2 as input */
    tmpccmr1 &= ((uint16_t)~TIM_CCMR1_CC1S) & ((uint16_t)~TIM_CCMR1_CC2S);
    tmpccmr1 |= TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0;

    /* Set the TI1 and the TI2 Polarities */
    tmpccer &= ((uint16_t)~TIM_CCER_CC1P) & ((uint16_t)~TIM_CCER_CC2P);
    tmpccer |= (uint16_t)(TIM_IC1Polarity | (uint16_t)(TIM_IC2Polarity << (uint16_t)4));

    /* Write to TIMx SMCR */
    TIMx->SMCR = tmpsmcr;

    /* Write to TIMx CCMR1 */
    TIMx->CCMR1 = tmpccmr1;

    /* Write to TIMx CCER */
    TIMx->CCER = tmpccer;
}

void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *TIM_TimeBaseInitStruct)
{
    /* Set the default configuration */
    TIM_TimeBaseInitStruct->TIM_Period = 0xFFFFFFFF;
    TIM_TimeBaseInitStruct->TIM_Prescaler = 0x0000;
    TIM_TimeBaseInitStruct->TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct->TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct->TIM_RepetitionCounter = 0x0000;
}

void TIM_TimeBaseInit(TIM_TypeDef *TIMx, TIM_TimeBaseInitTypeDef *TIM_TimeBaseInitStruct)
{
    uint16_t tmpcr1 = 0;

    tmpcr1 = TIMx->CR1;

    if ((TIMx == TIM1) || (TIMx == TIM8) ||
        (TIMx == TIM2) || (TIMx == TIM3) ||
        (TIMx == TIM4) || (TIMx == TIM5))
    {
        /* Select the Counter Mode */
        tmpcr1 &= (uint16_t)(~(TIM_CR1_DIR | TIM_CR1_CMS));
        tmpcr1 |= (uint32_t)TIM_TimeBaseInitStruct->TIM_CounterMode;
    }

    if ((TIMx != TIM6) && (TIMx != TIM7))
    {
        /* Set the clock division */
        tmpcr1 &= (uint16_t)(~TIM_CR1_CKD);
        tmpcr1 |= (uint32_t)TIM_TimeBaseInitStruct->TIM_ClockDivision;
    }

    TIMx->CR1 = tmpcr1;

    /* Set the Autoreload value */
    TIMx->ARR = TIM_TimeBaseInitStruct->TIM_Period;

    /* Set the Prescaler value */
    TIMx->PSC = TIM_TimeBaseInitStruct->TIM_Prescaler;

    if ((TIMx == TIM1) || (TIMx == TIM8))
    {
        /* Set the Repetition Counter value */
        TIMx->RCR = TIM_TimeBaseInitStruct->TIM_RepetitionCounter;
    }

    /* Generate an update event to reload the Prescaler
       and the repetition counter(only for TIM1 and TIM8) value immediatly */
    TIMx->EGR = TIM_PSCReloadMode_Immediate;
}

TIM_TimeBaseInitTypeDef TIMER_InitStructure;
TIM_TimeBaseInitTypeDef TIMER_InitStructureE;
TIM_TimeBaseInitTypeDef TIMER_InitStructureEE;
TIM_TimeBaseInitTypeDef TIMER_InitStructureEEG;

void TIM_Cmd(TIM_TypeDef *TIMx, FunctionalState NewState)
{

    if (NewState != DISABLE)
    {
        /* Enable the TIM Counter */
        TIMx->CR1 |= TIM_CR1_CEN;
    }
    else
    {
        /* Disable the TIM Counter */
        TIMx->CR1 &= (uint16_t)~TIM_CR1_CEN;
    }
}
void GPIO_PinAF(GPIO_TypeDef *GPIOx, uint16_t GPIO_PinSource, uint8_t GPIO_AF)

{

    uint32_t temp = 0x00;
    uint32_t temp_2 = 0x00;

    temp = ((uint32_t)(GPIO_AF) << ((uint32_t)((uint32_t)GPIO_PinSource & (uint32_t)0x07) * 4));
    GPIOx->AFR[GPIO_PinSource >> 0x03] &= ~((uint32_t)0xF << ((uint32_t)((uint32_t)GPIO_PinSource & (uint32_t)0x07) * 4));
    temp_2 = GPIOx->AFR[GPIO_PinSource >> 0x03] | temp;
    GPIOx->AFR[GPIO_PinSource >> 0x03] = temp_2;
}

void rcc_config()
{
    RCC->AHB1ENR |= 0x1;  // GPIOA
    RCC->AHB1ENR |= 0x4;  // GPIOC
    RCC->AHB1ENR |= 0x8;  // GPIOD
    RCC->AHB1ENR |= 0x10; // GPIOE

    RCC->APB1ENR |= 0x20000000; // ENABLE DAC
    RCC->APB2ENR |= 0x00000002; // APB2 TIM8
    RCC->APB1ENR |= 0x00000004; // APB1 TIM4
    RCC->APB1ENR |= 0x00000001; // APB1 TIM2
    RCC->APB1ENR |= 0x00000002; // APB1 TIM3

    GpioConfigPortA(GPIOA);
    GpioConfigPortC(GPIOC);
    GpioConfigPortD(GPIOD);

    GPIO_PinAF(GPIOA, GPIO_PinSource6, GPIO_AF_TIM3);
    GPIO_PinAF(GPIOA, GPIO_PinSource7, GPIO_AF_TIM3);

    GPIO_PinAF(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8);
    GPIO_PinAF(GPIOC, GPIO_PinSource7, GPIO_AF_TIM8);

    GPIO_PinAF(GPIOD, GPIO_PinSource12, GPIO_AF_TIM4);
    GPIO_PinAF(GPIOD, GPIO_PinSource13, GPIO_AF_TIM4);

    GPIO_PinAF(GPIOA, GPIO_PinSource0, GPIO_AF_TIM2);
    GPIO_PinAF(GPIOA, GPIO_PinSource1, GPIO_AF_TIM2);

    TIM_EncoderInterConfig(TIM8, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Falling);
    TIMER_InitStructure.TIM_Period = 65535;
    TIMER_InitStructure.TIM_CounterMode = TIM_CounterMode_Up | TIM_CounterMode_Down;
    TIM_TimeBaseInit(TIM8, &TIMER_InitStructure);
    TIM_TimeBaseStructInit(&TIMER_InitStructure);
    TIM_Cmd(TIM8, ENABLE);
    TIM8->CNT = 0;

    TIM_EncoderInterConfig(TIM4, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Falling);
    TIMER_InitStructureE.TIM_Period = 65535;
    TIMER_InitStructureE.TIM_CounterMode = TIM_CounterMode_Up | TIM_CounterMode_Down;
    TIM_TimeBaseInit(TIM4, &TIMER_InitStructureE);
    TIM_TimeBaseStructInit(&TIMER_InitStructureE);
    TIM_Cmd(TIM4, ENABLE);
    TIM4->CNT = 0;

    TIM_EncoderInterConfig(TIM2, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Falling);
    TIMER_InitStructureEE.TIM_Period = 65535;
    TIMER_InitStructureEE.TIM_CounterMode = TIM_CounterMode_Up | TIM_CounterMode_Down;
    TIM_TimeBaseInit(TIM2, &TIMER_InitStructureEE);
    TIM_TimeBaseStructInit(&TIMER_InitStructureEE);
    TIM_Cmd(TIM2, ENABLE);

    TIM2->CNT = 0;

    TIM_EncoderInterConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Falling);
    TIMER_InitStructureEEG.TIM_Period = 65535;
    TIMER_InitStructureEEG.TIM_CounterMode = TIM_CounterMode_Up | TIM_CounterMode_Down;
    TIM_TimeBaseInit(TIM3, &TIMER_InitStructureEEG);
    TIM_TimeBaseStructInit(&TIMER_InitStructureEEG);
    TIM_Cmd(TIM3, ENABLE);

    TIM3->CNT = 0;
}