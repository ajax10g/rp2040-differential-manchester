#include "hardware/pio.h"
#include "differential_manchester.pio.h"

#define RXBUF_SIZE 256
#define TXBUF_SIZE 256
#define DMA_XFER 16
#define RID_TXXFER 0xff

extern int buf_to_pio_sm_put_blocking(uint8_t* p, uint16_t len);
extern void dma_tx_setup(void);
extern void dma_rx_setup(void);

extern const uint pin_tx;
extern const uint pin_rx;
extern PIO pio;
extern uint sm_tx;
extern uint sm_rx;
extern uint32_t RxBuffer[RXBUF_SIZE];
extern uint32_t TxBuffer[TXBUF_SIZE];
extern int dma_rx_chan;
extern int dma_tx_chan;
extern uint32_t TxXferCount;
