/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <sys/types.h>

#include "differential_manchester.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

void dma_rx_handler(void);
void dma_tx_handler(void);
void dma_tx_setup(void);

// Differential serial transmit/receive example
// Need to connect a wire from GPIO14 -> GPIO15

const uint pin_tx = 14;
const uint pin_rx = 15;
PIO pio = pio0;
uint sm_tx = 0;
uint sm_rx = 1;

/*Aligned for the DMA ring address wrap*/
uint32_t RxBuffer[RXBUF_SIZE] __attribute__((aligned(sizeof(RxBuffer) * 1))) = { [0 ... RXBUF_SIZE - 1] = 0 };
int dma_rx_chan;

void dma_rx_handler(void)
{
    // clear the interrupt request
    dma_channel_acknowledge_irq1(dma_rx_chan);

    // Restart the dma
    dma_channel_start(dma_rx_chan);
}

/*Aligned for the DMA ring address wrap*/
uint32_t TxBuffer[TXBUF_SIZE] __attribute__((aligned(sizeof(TxBuffer) * 1))) = { [0 ... TXBUF_SIZE - 1] = 0 };
int dma_tx_chan;
uint32_t TxXferCount = 0;
void dma_tx_setup(void)
{
    uint8_t* p = (uint8_t*)TxBuffer;
    for (int i = 0; i < TXBUF_SIZE * sizeof(TxBuffer[0]); i++)
        *p++ = i % 0x100;
#if 0
    printf("TxBuffer=\n");
    p = (uint8_t*)TxBuffer;
    for (int i = 0; i < TXBUF_SIZE * sizeof(TxBuffer[0]); i++)
        printf("%02x ", *p++);
    printf("\n");
#endif

    dma_tx_chan = dma_claim_unused_channel(true);
    // Setup the tx channel
    dma_channel_config c0 = dma_channel_get_default_config(dma_tx_chan); // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);             // 32-bit txfers
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing
    channel_config_set_ring(&c0, false /*read*/, 10 /*256=(1<<10)/4*/);  // write address wrapping
    channel_config_set_dreq(&c0, pio_get_dreq(pio, sm_tx, true));        // paced by PIO tx fifo

    dma_channel_configure(dma_tx_chan, // Channel to be configured
        &c0,                           // The configuration we just created
        &pio->txf[sm_tx],              // write address
        TxBuffer,                      // Read address WRAPPED
        DMA_XFER,                      // Number of transfers
        false                          // Don't start immediately
    );

    dma_channel_set_irq0_enabled(dma_tx_chan, true); // raise IRQ line 0 when the channel finishes a block
    irq_set_exclusive_handler(DMA_IRQ_0, dma_tx_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void dma_tx_handler(void)
{
    /*Update the total count*/
    TxXferCount += DMA_XFER;

    /*clear the interrupt request*/
    dma_channel_acknowledge_irq0(dma_tx_chan);
}

int buf_to_pio_sm_put_blocking(uint8_t* p, uint16_t len)
{
    uint32_t* tx_buffer_pointer = (u_int32_t*)p;
    int xfer_count = len / sizeof(uint32_t);

    if (len < 64 | len % 4) {
        printf("buf_to_pio_sm_put_blocking: Error len=%d\n", len);
        return len; // Error
    }

    // printf("buf_to_pio_sm_put_blocking: Write to dma_tx_chan's read_addr=%04x, xfer_count=%d\n", (uint32_t)p, xfer_count);
    dma_channel_set_read_addr(dma_tx_chan, (void*)p, false /*trigger*/);
    dma_channel_set_trans_count(dma_tx_chan, xfer_count, true /*trigger*/);
    dma_channel_wait_for_finish_blocking(dma_tx_chan);
    return 0;
}

void dma_rx_setup(void)
{
    // disable the sm
    pio_sm_set_enabled(pio, sm_rx, false);
    // make sure the FIFOs are empty
    pio_sm_clear_fifos(pio, sm_rx);

    // Select DMA channels
    dma_rx_chan = dma_claim_unused_channel(true);

    // Setup the rx channel
    dma_channel_config c1 = dma_channel_get_default_config(dma_rx_chan); // Default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);             // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                       // no read incrementing
    channel_config_set_write_increment(&c1, true);                       // yes write incrementing

    // Can only have either write or read address wrapping
    channel_config_set_ring(&c1, true /*write*/, 10 /*256=(1<<10)/4*/); // write address wrapping

    // let the sm_rx of pio determine the speed
    channel_config_set_dreq(&c1, pio_get_dreq(pio, sm_rx, false /*rx*/));

    dma_channel_configure(dma_rx_chan, // Channel to be configured
        &c1,                           // The configuration we just created
        RxBuffer,                      // write address WRAPPED
        &pio->rxf[sm_rx],              // read address: the output of sm_rx
        DMA_XFER,                      // Number of transfers
        false                          // Don't start immediately.
    );
    // Tell the DMA to raise IRQ line 1 when the channel finishes a block
    dma_channel_set_irq1_enabled(dma_rx_chan, true);

    // Configure the processor to run dma_rx_handler() when DMA IRQ 1 is asserted
    irq_set_exclusive_handler(DMA_IRQ_1, dma_rx_handler);
    irq_set_enabled(DMA_IRQ_1, true);

    // enable the sm
    pio_sm_set_enabled(pio, sm_rx, true);
}
