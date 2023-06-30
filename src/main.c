/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "differential_manchester.h"
#include "hardware/dma.h"
#include "pico/multicore.h"
#include "usb_descriptors.h"

/* This example demonstrate HID Generic raw Input & Output.
 * It will receive data from Host (In endpoint) and echo back (Out endpoint).
 * HID Report descriptor use vendor for usage page (using template TUD_HID_REPORT_DESC_GENERIC_INOUT)
 *
 * There are 2 ways to test the sketch
 * 1. Using nodejs
 * - Install nodejs and npm to your PC
 *
 * - Install excellent node-hid (https://github.com/node-hid/node-hid) by
 *   $ npm install node-hid
 *
 * - Run provided hid test script
 *   $ node hid_test.js
 *
 * 2. Using python
 * - Install `hid` package (https://pypi.org/project/hid/) by
 *   $ pip install hid
 *
 * - hid package replies on hidapi (https://github.com/libusb/hidapi) for backend,
 *   which already available in Linux. However on windows, you may need to download its dlls from their release page and
 *   copy it over to folder where python is installed.
 *
 * - Run provided hid test script to send and receive data to this device.
 *   $ python3 hid_test.py
 */

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void core1_entry(void);
void dma_rx_task(void);

/*------------- MAIN -------------*/
int main(void)
{
    stdio_init_all();
    board_init();

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    printf("\033[2J"); // Clear screen
    printf("USB HID To Differential Manchester (Bi-Phase Mark Code FM1)\n");

    uint offset_tx = pio_add_program(pio, &differential_manchester_tx_program);
    uint offset_rx = pio_add_program(pio, &differential_manchester_rx_program);
    printf("Transmit program loaded at %d\n", offset_tx);
    printf("Receive program loaded at %d\n", offset_rx);

    // Configure state machines, set bit rate at 6 Mbps
    differential_manchester_tx_program_init(pio, sm_tx, offset_tx, pin_tx, 125.f / (16 * 6));
    differential_manchester_rx_program_init(pio, sm_rx, offset_rx, pin_rx, 125.f / (16 * 6));
    pio_sm_set_enabled(pio, sm_tx, true);

    pio_sm_set_enabled(pio, sm_rx, true);

    multicore_launch_core1(core1_entry);

    dma_tx_setup();

    while (1) {
        tud_task(); // tinyusb device task
        led_blinking_task();
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) { blink_interval_ms = BLINK_MOUNTED; }

// Invoked when device is unmounted
void tud_umount_cb(void) { blink_interval_ms = BLINK_NOT_MOUNTED; }

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) { blink_interval_ms = BLINK_MOUNTED; }

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    /*return 0;*/

    switch (report_id) {
    case RID_TXXFER:
        printf("tud_hid_get_report_cb:  itf=0x%02X, report_id=0x%02X, report_type=0x%02X, size=%d\n", itf, report_id, report_type, TxXferCount);
        int byte_count = sizeof(TxXferCount);
        for (int i = 0; i < byte_count; i++)
            buffer[i] = TxXferCount >> i * 8 & 0xff;
        return byte_count;
    default:
        return 0;
    }
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    // This example doesn't use multiple report and report ID
    (void)itf;
    (void)report_id;
    (void)report_type;

    /*clpham:*/
    int result = buf_to_pio_sm_put_blocking((uint8_t*)buffer, bufsize);
    if (result > 0)
        printf("tud_hid_set_report: Error; len = %d; SHOULD BE MULTIPLE OF 64\n", result);

#if 0
    // echo back anything we received from host
    tud_hid_report(0, buffer, bufsize);
#endif
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // Blink every interval ms
    if (board_millis() - start_ms < blink_interval_ms)
        return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;

#if 0
    uint8_t next_report_id = report[0] + 1u;
    if (next_report_id<REPORT_ID_COUNT){
        // Do something ...
    }
#endif
}

void core1_entry(void)
{
    dma_rx_setup();
    while (1) {
        dma_rx_task();
    }
}

void dma_rx_task(void)
{
    static uint offset = 0; // units of entries
    uint32_t* p = RxBuffer;

    /*Point to fresh data*/
    p += offset;

    /*Start/restart dma channel*/
    dma_channel_start(dma_rx_chan);

    /*Wait until dma xfer has completed*/
    dma_channel_wait_for_finish_blocking(dma_rx_chan);

    /*Send report to the usb host*/
    tud_hid_report(0, p, 64);

    /*Update offset to the next fresh data region*/
    offset = (offset + DMA_XFER) % RXBUF_SIZE;
}
