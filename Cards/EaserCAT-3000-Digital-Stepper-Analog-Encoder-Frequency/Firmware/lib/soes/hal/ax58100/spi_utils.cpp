#include "spi_utils.h"
// #include <Arduino.h>
#include <SPI.h>


char SCS = ESC_GPIO_Pin_CS;


void spi_setup(void)
{
  SPI.begin();    
  pinMode(SCS, OUTPUT);   
  spi_unselect(0);
  delay(100);     
  SPI.beginTransaction(SPISettings(SPIX_ESC_SPEED, MSBFIRST, SPI_MODE3)); 

}

void spi_select (int8_t board)
{
    // Soft CSN
    #if SCS_ACTIVE_POLARITY == SCS_LOW
    digitalWrite(SCS, LOW);
    #endif
}

void spi_unselect (int8_t board)
{
    // Soft CSN
    #if SCS_ACTIVE_POLARITY == SCS_LOW
    digitalWrite(SCS, HIGH);
    #endif
}

// Whole blocks per call: a call per byte cost about 2 us per byte, the SPI clock itself
// (42 MHz) only 0.2 us.
void spi_write (int8_t board, uint8_t *data, uint16_t size)
{
    SPI.transfer(static_cast<const void *>(data), static_cast<void *>(nullptr), size);
}

void spi_read (int8_t board, uint8_t *result, uint16_t size)
{
    SPI.transfer(static_cast<const void *>(nullptr), static_cast<void *>(result), size);  // NULL: clocks out DUMMY_BYTE (0xFF)
}


void spi_bidirectionally_transfer (int8_t board, uint8_t *result, uint8_t *data, uint16_t size)
{
    SPI.transfer(static_cast<const void *>(data), static_cast<void *>(result), size);
}
