#include <Arduino.h>
#include "rst.h"

void rst_setup(void)
{
    /* Set RSTN as ouput */
    pinMode(ESC_Pin_RSTN, OUTPUT);
    rst_high();
}

void rst_low(void)
{ /* Set RSTN line low */
    digitalWrite(ESC_Pin_RSTN, LOW);
}

void rst_high(void)
{
    /* Set RSTN line high */
    digitalWrite(ESC_Pin_RSTN, HIGH);
}

void rst_check_start(void)
{
    /* Setup NRST as GPIO input and pull it high */
    pinMode(ESC_Pin_RSTN, INPUT_PULLUP);
}

uint8_t is_esc_reset(void)
{
    /* Check if ESC pulled RSTN line up */
    return digitalRead(ESC_Pin_RSTN);
}
