#include "spi_utils.h"
#include <SPI.h>

// SPI to the AX58100 on SPI1 (PA5 SCK, PA6 MISO, PA7 MOSI), CS on PC4, mode 3, 42 MHz.
//
// SPI.begin()/beginTransaction() set the peripheral up; the transfers are done here at
// register level. SPI.transfer() and a plain register loop both wait out every byte
// (0.57 us per byte at 42 MHz, 0.19 us on the wire). Transfers of SPI_DMA_MIN bytes or
// more use DMA2 (SPI1_RX stream 2, SPI1_TX stream 3, channel 3) and run at the wire
// speed; interrupts stay enabled, and the DMA keeps going while one is served. Short
// transfers (addresses, single registers) use a loop that waits for each byte, which
// cannot overrun even if interrupted.

char SCS = ESC_GPIO_Pin_CS;

static GPIO_TypeDef *csPort = nullptr;
static uint32_t csMask = 0;
static const uint16_t SPI_DMA_MIN = 8;
static const uint8_t dmaDummyTx = DUMMY_BYTE;
static uint8_t dmaDummyRx;

void spi_setup(void)
{
  SPI.begin();
  pinMode(SCS, OUTPUT);
  csPort = digitalPinToPort(SCS);
  csMask = digitalPinToBitMask(SCS);
  spi_unselect(0);
  delay(100);
  SPI.beginTransaction(SPISettings(SPIX_ESC_SPEED, MSBFIRST, SPI_MODE3));
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  (void)RCC->AHB1ENR;
}

void spi_select (int8_t board)
{
    // Soft CSN
    #if SCS_ACTIVE_POLARITY == SCS_LOW
    if (csPort) csPort->BSRR = csMask << 16;
    else digitalWrite(SCS, LOW);
    #endif
}

void spi_unselect (int8_t board)
{
    // Soft CSN
    #if SCS_ACTIVE_POLARITY == SCS_LOW
    if (csPort) csPort->BSRR = csMask;
    else digitalWrite(SCS, HIGH);
    #endif
}

// One byte at a time: write, wait for the answer, read
static void spi_loop(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    SPI_TypeDef *s = SPI1;
    for (uint16_t i = 0; i < n; i++) {
        while (!(s->SR & SPI_SR_TXE)) {}
        *(volatile uint8_t *)&s->DR = tx ? tx[i] : DUMMY_BYTE;
        while (!(s->SR & SPI_SR_RXNE)) {}
        uint8_t b = *(volatile uint8_t *)&s->DR;
        if (rx) rx[i] = b;
    }
    while (s->SR & SPI_SR_BSY) {}
}

static inline bool dmaReachable(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    return !(a >= 0x10000000u && a < 0x10010000u);  // the CCM RAM has no DMA access
}

#define DMA_FLAGS_S2 (DMA_LIFCR_CFEIF2 | DMA_LIFCR_CDMEIF2 | DMA_LIFCR_CTEIF2 | DMA_LIFCR_CHTIF2 | DMA_LIFCR_CTCIF2)
#define DMA_FLAGS_S3 (DMA_LIFCR_CFEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTCIF3)

static void spi_dma(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    SPI_TypeDef *s = SPI1;
    DMA_Stream_TypeDef *rxs = DMA2_Stream2;
    DMA_Stream_TypeDef *txs = DMA2_Stream3;

    rxs->CR = 0;
    txs->CR = 0;
    while ((rxs->CR & DMA_SxCR_EN) || (txs->CR & DMA_SxCR_EN)) {}
    DMA2->LIFCR = DMA_FLAGS_S2 | DMA_FLAGS_S3;

    rxs->PAR = (uint32_t)&s->DR;
    rxs->M0AR = (uint32_t)(rx ? rx : &dmaDummyRx);
    rxs->NDTR = n;
    rxs->FCR = 0;
    rxs->CR = (3u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 | (rx ? DMA_SxCR_MINC : 0);  // periph -> mem
    txs->PAR = (uint32_t)&s->DR;
    txs->M0AR = (uint32_t)(tx ? tx : &dmaDummyTx);
    txs->NDTR = n;
    txs->FCR = 0;
    txs->CR = (3u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 | DMA_SxCR_DIR_0 |
              (tx ? DMA_SxCR_MINC : 0);                                         // mem -> periph

    (void)s->DR;  // nothing stale in the receive register
    (void)s->SR;
    rxs->CR |= DMA_SxCR_EN;
    txs->CR |= DMA_SxCR_EN;
    s->CR2 |= SPI_CR2_RXDMAEN;
    s->CR2 |= SPI_CR2_TXDMAEN;

    while (!(DMA2->LISR & (DMA_LISR_TCIF2 | DMA_LISR_TEIF2))) {}
    while (s->SR & SPI_SR_BSY) {}
    s->CR2 &= ~(SPI_CR2_RXDMAEN | SPI_CR2_TXDMAEN);
    rxs->CR = 0;
    txs->CR = 0;
    DMA2->LIFCR = DMA_FLAGS_S2 | DMA_FLAGS_S3;
}

static inline void spi_transfer_fast(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    if (n >= SPI_DMA_MIN && (!tx || dmaReachable(tx)) && (!rx || dmaReachable(rx)))
        spi_dma(tx, rx, n);
    else
        spi_loop(tx, rx, n);
}

void spi_write (int8_t board, uint8_t *data, uint16_t size)
{
    spi_transfer_fast(data, nullptr, size);
}

void spi_read (int8_t board, uint8_t *result, uint16_t size)
{
    spi_transfer_fast(nullptr, result, size);  // clocks out DUMMY_BYTE (0xFF)
}

void spi_bidirectionally_transfer (int8_t board, uint8_t *result, uint8_t *data, uint16_t size)
{
#ifdef CYCLE_PROFILE
    if (!spi_use_fast) {  // the old way, for comparison
        SPI.transfer(static_cast<const void *>(data), static_cast<void *>(result), size);
        return;
    }
#endif
    spi_transfer_fast(data, result, size);
}

#ifdef CYCLE_PROFILE
volatile uint8_t spi_use_fast = 1;
void spi_fast_transfer(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    spi_transfer_fast(tx, rx, n);
}
#endif
