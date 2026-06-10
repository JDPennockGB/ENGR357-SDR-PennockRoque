#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

 // fixed verson for new drivers
 
#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENABLED         1
#define CFG_TUD_ENDPOINT0_SIZE  64

#define CFG_TUD_CDC             1
#define CFG_TUD_CDC_RX_BUFSIZE  256
#define CFG_TUD_CDC_TX_BUFSIZE  256
#define CFG_TUD_CDC_EP_BUFSIZE  64

#define CFG_TUD_AUDIO           1
// Exact memory reservations required for the raw descriptor
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN        112
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT        1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ     64

#define CFG_TUD_AUDIO_ENABLE_EP_IN           1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX    392
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ 1536

#define CFG_TUD_HID     0
#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

#ifdef __cplusplus
}
#endif
#endif
