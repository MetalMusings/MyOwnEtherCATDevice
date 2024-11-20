#ifndef SRC_APP_SPI_H_
#define SRC_APP_SPI_H_

#include <stdint.h>

#define SCS_LOW                           LOW
#define SCS_HIGH                          HIGH
#define SCS_ACTIVE_POLARITY               SCS_LOW

// 80 MHz
#define SPIX_ESC_SPEED                    80000000
#define ESC_GPIO_Pin_CS                   PC4

#define DUMMY_BYTE 0xFF
#ifdef __cplusplus
extern "C" {
#endif

void spi_setup(void);
void spi_select (int8_t board);
void spi_unselect (int8_t board);
void spi_write (int8_t board, uint8_t *data, uint8_t size);
void spi_read (int8_t board, uint8_t *result, uint8_t size);
void spi_bidirectionally_transfer (int8_t board, uint8_t *result, uint8_t *data, uint8_t size);

#ifdef __cplusplus
}
#endif


#endif /* SRC_APP_SPI_H_ */
