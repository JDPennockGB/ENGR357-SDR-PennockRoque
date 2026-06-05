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
#include "si5351.h" // <--- IMPORTANT: Required for compilation

void cdc_send(const char* str);

#define ENC_A_PIN 20
#define ENC_B_PIN 21

// --- Hardware Pins ---
#define LED_PIN      25
#define I2C_SDA_PIN  12
#define I2C_SCL_PIN  13
#define I2S_DATA_PIN 14  
#define FMT_PIN      10  
#define MD1_PIN      11  

// --- State Variables ---
static uint32_t current_hz = 0;
static char current_status[16] = "OK,X,0"; 
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
    } else {
        return;
    }
    multicore_fifo_push_timeout_us((uint32_t)(uintptr_t)filled_buf, 0);
}

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
        for (int i = 0; i < WORDS_PER_BUF; i++) {
            src[i] &= 0xFFFFFF00; 
        }
        if (s_audio_alt == 1) {
            tud_audio_write(src, SINGLE_BUFFER_SIZE);
        }
    }
}

void encoder_callback(uint gpio, uint32_t events) {
    gpio_xor_mask(1u << LED_PIN); 
}

void cdc_send(const char* str) {
    tud_cdc_write(str, strlen(str));
    tud_cdc_write_flush();
}

void cdc_task(void) {
    if (!tud_cdc_connected()) {
        sentinel_sent = false; 
        return;
    }
    if (!sentinel_sent) {
        cdc_send("SDR ready\n");
        sentinel_sent = true;
    }
    if (tud_cdc_available()) {
        char buf[64];
        uint32_t count = tud_cdc_read(buf, sizeof(buf) - 1);
        buf[count] = '\0';

        if (buf[0] == 0x03 || buf[0] == 0x04) return;

        if (strncmp(buf, "VER", 3) == 0) cdc_send("VER,1.0\nOK\n");
        else if (strncmp(buf, "XTAL", 4) == 0) cdc_send("XTAL,24576000\nOK\n");
        else if (strncmp(buf, "MODE", 4) == 0) cdc_send("MODE,DIRECT\nOK\n");
        else if (strncmp(buf, "RATE", 4) == 0) cdc_send("OK\n");
        else if (strncmp(buf, "FREQ,", 5) == 0) {
            if (strlen(buf) <= 7) {
                char resp[64];
                snprintf(resp, sizeof(resp), "%lu\n%s\n", current_hz, current_status);
                cdc_send(resp);
            } else {
                uint32_t hz, n, a, b, c, p1, p2, p3;
                if (sscanf(buf, "FREQ,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu", &hz, &n, &a, &b, &c, &p1, &p2, &p3) == 8) {
                    // CORRECTED: Now using the correct library function
                    si5351_set_frequency(i2c0, SI5351_CLK0, hz, SI5351_INTEGER_APPROX, SI5351_CLK_NONE); 
                    
                    current_hz = hz;
                    strcpy(current_status, (b == 0) ? "OK,G,0" : "OK,F,0");
                    char resp[64];
                    snprintf(resp, sizeof(resp), "%lu\n%s\n", current_hz, current_status);
                    cdc_send(resp);
                } else cdc_send("ERR\n");
            }
        } else if (buf[0] != '\r' && buf[0] != '\n') cdc_send("ERR\n");
    }
}

int main(void) {
    board_init();
    tusb_init();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(ENC_A_PIN); gpio_set_dir(ENC_A_PIN, GPIO_IN); gpio_pull_up(ENC_A_PIN);
    gpio_init(ENC_B_PIN); gpio_set_dir(ENC_B_PIN, GPIO_IN); gpio_pull_up(ENC_B_PIN);
    gpio_set_irq_enabled_with_callback(ENC_A_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &encoder_callback);
    gpio_set_irq_enabled_with_callback(ENC_B_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &encoder_callback);

    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    // CORRECTED: Ensure init is called
    si5351_init(i2c0);

    gpio_init(FMT_PIN); gpio_set_dir(FMT_PIN, GPIO_OUT); gpio_put(FMT_PIN, 0);  
    gpio_init(MD1_PIN); gpio_set_dir(MD1_PIN, GPIO_OUT); gpio_put(MD1_PIN, 0);  

    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &i2s_rx_program);
    i2s_rx_program_init(pio, sm, offset, I2S_DATA_PIN, I2S_DATA_PIN + 1, I2S_DATA_PIN + 2);
    pio_sm_set_enabled(pio, sm, true);

    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);
    
    dma_channel_config ca = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&ca, DMA_SIZE_32);
    channel_config_set_read_increment(&ca, false);
    channel_config_set_write_increment(&ca, true);
    channel_config_set_dreq(&ca, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&ca, dma_chan_b);
    dma_channel_configure(dma_chan_a, &ca, buf_a, &pio->rxf[sm], WORDS_PER_BUF, false);

    dma_channel_config cb = dma_channel_get_default_config(dma_chan_b);
    channel_config_set_transfer_data_size(&cb, DMA_SIZE_32);
    channel_config_set_read_increment(&cb, false);
    channel_config_set_write_increment(&cb, true);
    channel_config_set_dreq(&cb, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&cb, dma_chan_a);
    dma_channel_configure(dma_chan_b, &cb, buf_b, &pio->rxf[sm], WORDS_PER_BUF, false);

    multicore_launch_core1(core1_entry);   

    while (1) {
        tud_task();    
        cdc_task();    
        audio_task();  
    }
    return 0;
}

// --- USB Audio Callbacks (Fixed) ---
// --- USB Audio Callbacks (Fixed) ---
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport; 
    s_audio_alt = (uint8_t)(p_request->wValue & 0xFF); 
    return true;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport; (void)p_request; 
    return true;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    if ((p_request->wValue >> 8) == 0x01) { // 0x01 = SAMPLING_FREQ control
        uint8_t req = p_request->bRequest;
        
        // 0x81 = GET_CUR, 0x82 = GET_MIN, 0x83 = GET_MAX
        if (req == 0x81 || req == 0x82 || req == 0x83) { 
            uint8_t freq[3] = {(uint8_t)(s_sample_rate_hz & 0xFFu), 
                               (uint8_t)((s_sample_rate_hz >>  8) & 0xFFu), 
                               (uint8_t)((s_sample_rate_hz >> 16) & 0xFFu)};
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, freq, sizeof(freq));
        } 
        // 0x84 = GET_RES
        else if (req == 0x84) { 
            uint8_t res[3] = {0, 0, 0}; // 0 Hz resolution for a fixed frequency
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, res, sizeof(res));
        }
    }
    return false; // Stall anything else
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
    (void)rhport;
    // Only respond to 0x01 (SET_CUR)
    if (p_request->bRequest == 0x01 && (p_request->wValue >> 8) == 0x01 && p_request->wLength == 3) {
        s_sample_rate_hz = ((uint32_t)pBuff[2] << 16) | ((uint32_t)pBuff[1] <<  8) |  (uint32_t)pBuff[0];
        return true;
    }
    return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t channelNum = (uint8_t)(p_request->wValue & 0xFF);
    if ((p_request->wValue >> 8) == 0x01 && channelNum < 3u) { // 0x01 = MUTE
        // Boolean controls only support GET_CUR
        if (p_request->bRequest == 0x81) { 
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_mute[channelNum], 1);
        }
    }
    return false; 
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff) {
    (void)rhport; uint8_t channelNum = (uint8_t)(p_request->wValue & 0xFF); 
    // Only allow SET_CUR
    if (p_request->bRequest == 0x01 && (p_request->wValue >> 8) == 0x01 && channelNum < 3u) { 
        s_mute[channelNum] = pBuff[0]; 
        return true;
    } 
    return false;
}