/**
 * SDR Controller - Main Firmware
 * Pinout Configured: DOUT=GPIO2, SD=GPIO11, LCD=GPIO3-10,14,15
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/i2c.h"
#include "tusb.h"
#include "bsp/board.h"
#include "i2s_rx.pio.h"
#include "si5351.h"
#include "lcd_driver.h"
#include "class/audio/audio_device.h"

// --- Hardware Pins ---
#define ENC_A_PIN    20
#define ENC_B_PIN    21
#define LED_PIN      25
#define I2C_SDA_PIN  12
#define I2C_SCL_PIN  13
#define I2S_DATA_PIN 2    // DOUT
#define I2S_BCK_PIN  0
#define I2S_WS_PIN   1

// --- State Variables ---
static volatile uint32_t current_hz = 5680000; 
static volatile bool frequency_changed = true;
static bool sentinel_sent = false;

static uint8_t  s_audio_alt      = 0;
static uint32_t s_sample_rate_hz = 48000U;
static uint8_t  s_mute[3]        = {0, 0, 0}; 

// --- DMA & Audio Buffers ---
#define SINGLE_BUFFER_SIZE  768             
#define WORDS_PER_BUF       (SINGLE_BUFFER_SIZE / 4)  
static uint32_t buf_a[WORDS_PER_BUF];
static uint32_t buf_b[WORDS_PER_BUF];
static uint dma_chan_a, dma_chan_b;

// --- ISRs ---
static void __not_in_flash_func(dma_handler)(void) {
    const uint32_t *filled_buf;
    if (dma_channel_get_irq0_status(dma_chan_a)) {
        dma_channel_acknowledge_irq0(dma_chan_a);
        filled_buf = buf_a;
        dma_channel_set_write_addr(dma_chan_a, buf_a, false);
    } else if (dma_channel_get_irq0_status(dma_chan_b)) {
        dma_channel_acknowledge_irq0(dma_chan_b);
        filled_buf = buf_b;
        dma_channel_set_write_addr(dma_chan_b, buf_b, false);
    } else return;
    multicore_fifo_push_timeout_us((uint32_t)(uintptr_t)filled_buf, 0);
}

void encoder_callback(uint gpio, uint32_t events) {
    if (gpio == ENC_A_PIN) {
        if (gpio_get(ENC_B_PIN)) current_hz += 40000;
        else current_hz -= 40000;
        frequency_changed = true;
    }
}

// --- Tasks ---
static void core1_entry(void) {
    multicore_fifo_drain();               
    irq_clear(DMA_IRQ_0);
    dma_channel_set_irq0_enabled(dma_chan_a, true);
    dma_channel_set_irq0_enabled(dma_chan_b, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_start(dma_chan_a);        
    while (true) { tight_loop_contents(); }
}

static void audio_task(void) {
    uint32_t ptr_val = 0;
    if (multicore_fifo_pop_timeout_us(0, &ptr_val) && ptr_val != 0) {
        uint32_t *src = (uint32_t *)(uintptr_t)ptr_val;
        for (int i = 0; i < WORDS_PER_BUF; i++) src[i] &= 0xFFFFFF00; 
        if (s_audio_alt == 1) tud_audio_write(src, SINGLE_BUFFER_SIZE);
    }
}

void cdc_task(void) {
    if (!tud_cdc_connected()) { sentinel_sent = false; return; }
    if (!sentinel_sent) { 
        tud_cdc_write("SDR ready\n", 10);
        tud_cdc_write_flush();
        sentinel_sent = true; 
    }
}

int main(void) {
    // Replace board_init(); with:
    stdio_init_all();
    tusb_init();

    lcd_init();
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    si5351_init(i2c0);

    gpio_init(ENC_A_PIN); gpio_pull_up(ENC_A_PIN);
    gpio_init(ENC_B_PIN); gpio_pull_up(ENC_B_PIN);
    gpio_set_irq_enabled_with_callback(ENC_A_PIN, GPIO_IRQ_EDGE_RISE, true, &encoder_callback);

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &i2s_rx_program);
    // Your pins are: BCK=0, WS=1, DOUT=2. The base pin is 0.
// PIO will automatically assign: base+0=BCK, base+1=WS, base+2=DOUT.
    i2s_rx_program_init(pio, 0, offset, 0);
    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);
    
    dma_channel_config c = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, 0, false));
    dma_channel_configure(dma_chan_a, &c, buf_a, &pio->rxf[0], WORDS_PER_BUF, false);
    
    // ... Repeat for dma_chan_b ...

    multicore_launch_core1(core1_entry);

    while (1) {
        tud_task();    
        cdc_task();    
        audio_task();  

        // Inside while(1) loop in main.c
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);

        tud_task();
        cdc_task();
        audio_task();

        if (frequency_changed) {
            // Keep the oscillator running at the 4x frequency
            si5351_set_frequency(i2c0, SI5351_CLK0, current_hz, SI5351_INTEGER_APPROX, SI5351_CLK_NONE);
            
            // Calculate the actual tuned frequency for the display (divided by 4)
            uint32_t display_hz = current_hz / 4;
            
            // Format the string (convert Hz to kHz)
            char buf[16];
            snprintf(buf, sizeof(buf), "%4d kHz", (int)(display_hz / 1000));
            
            // Print to the LCD
            lcd_clear();
            lcd_print(buf); 
            frequency_changed = false;
        }
    }
}

// --- Audio Callbacks ---
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) { (void)rhport; s_audio_alt = (uint8_t)(p_request->wValue & 0xFF); return true; }
bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) { return true; }
bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) { return false; }
bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) { return true; }
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) { return false; }
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) { return true; }