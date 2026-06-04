#include "tusb.h"
#include "tusb_config.h"
#include <string.h>

enum {
    ITF_NUM_CDC_CONTROL = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_AUDIO_CONTROL,
    ITF_NUM_AUDIO_STREAMING,
    ITF_NUM_TOTAL
};

enum {
    STRID_MANUFACTURER = 1,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC,
    STRID_AUDIO
};

enum {
    EPNUM_CDC_NOTIF   = 0x83,
    EPNUM_CDC_OUT     = 0x04,
    EPNUM_CDC_IN      = 0x84,
    EPNUM_AUDIO_IN    = 0x81,
};

enum {
    AUDIO_CS_AC_INTERFACE_HEADER       = 0x01,
    AUDIO_CS_AC_INTERFACE_INPUT_TERM   = 0x02,
    AUDIO_CS_AC_INTERFACE_OUTPUT_TERM  = 0x03,
    AUDIO_CS_AC_INTERFACE_FEATURE_UNIT = 0x06,
    AUDIO_CS_AS_INTERFACE_GENERAL      = 0x01,
    AUDIO_CS_AS_FORMAT_TYPE            = 0x02,
    AUDIO_CS_EP_GENERAL                = 0x01
};

enum {
    AUDIO_ENTITY_INPUT_TERMINAL  = 0x01,
    AUDIO_ENTITY_FEATURE_UNIT    = 0x02,
    AUDIO_ENTITY_OUTPUT_TERMINAL = 0x03,
};

enum {
    AUDIO_TERMINAL_USB_STREAMING = 0x0101,
    AUDIO_TERMINAL_GENERIC_MIC   = 0x0201,
};

#define CONFIG_TOTAL_LEN             187
#define CONFIG_POWER_MA              50
#define CDC_NOTIFICATION_EP_SIZE     8
#define CDC_DATA_EP_SIZE             64
#define AUDIO_STREAM_EP_SIZE         392
#define AUDIO_POLL_INTERVAL_MS       1
#define AUDIO_AC_TOTAL_LEN           43
#define AUDIO_SAMPLE_RATE_HZ         48000

_Static_assert(CONFIG_TOTAL_LEN == 187, "Unexpected config descriptor length");
_Static_assert(CFG_TUD_AUDIO_FUNC_1_DESC_LEN == 112, "Audio function descriptor length must stay at 112");
_Static_assert(CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX == AUDIO_STREAM_EP_SIZE, "Audio endpoint size mismatch");

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0xEF,   // Misc Device
    .bDeviceSubClass    = 0x02,   // Common
    .bDeviceProtocol    = 0x01,   // IAD
    .bMaxPacketSize0    = 64,
    .idVendor           = 0xCAFE,
    .idProduct          = 0x4020, // Incremented to 0x4020 to bypass macOS cache
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1
};

uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&desc_device; }

// USB 1.1 full-speed composite configuration: CDC + UAC1.
static uint8_t const desc_configuration[] = {
    // Configuration descriptor
    0x09, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(CONFIG_TOTAL_LEN), ITF_NUM_TOTAL, 0x01, 0x00, 0xA0, CONFIG_POWER_MA,

    // CDC IAD (interfaces 0..1)
    0x08, TUSB_DESC_INTERFACE_ASSOCIATION, ITF_NUM_CDC_CONTROL, 0x02, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
    CDC_COMM_PROTOCOL_NONE, 0x00,

    // CDC control interface
    0x09, TUSB_DESC_INTERFACE, ITF_NUM_CDC_CONTROL, 0x00, 0x01, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,
    CDC_COMM_PROTOCOL_NONE, STRID_CDC,
    // CDC class-specific descriptors (Header, Call Management, ACM, Union)
    0x05, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120),
    0x05, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0x00, ITF_NUM_CDC_DATA,
    0x04, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 0x06,
    0x05, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, ITF_NUM_CDC_CONTROL, ITF_NUM_CDC_DATA,
    // CDC notification endpoint
    0x07, TUSB_DESC_ENDPOINT, EPNUM_CDC_NOTIF, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(CDC_NOTIFICATION_EP_SIZE), 0x01,

    // CDC data interface
    0x09, TUSB_DESC_INTERFACE, ITF_NUM_CDC_DATA, 0x00, 0x02, TUSB_CLASS_CDC_DATA, 0x00, 0x00, 0x00,
    0x07, TUSB_DESC_ENDPOINT, EPNUM_CDC_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(CDC_DATA_EP_SIZE), 0x00,
    0x07, TUSB_DESC_ENDPOINT, EPNUM_CDC_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(CDC_DATA_EP_SIZE), 0x00,

    // Audio IAD (interfaces 2..3)
    0x08, TUSB_DESC_INTERFACE_ASSOCIATION, ITF_NUM_AUDIO_CONTROL, 0x02, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL,
    AUDIO_INT_PROTOCOL_CODE_V1, 0x00,

    // AudioControl interface (UAC1)
    0x09, TUSB_DESC_INTERFACE, ITF_NUM_AUDIO_CONTROL, 0x00, 0x00, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL,
    AUDIO_INT_PROTOCOL_CODE_V1, STRID_AUDIO,
    0x09, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_HEADER, U16_TO_U8S_LE(0x0100), U16_TO_U8S_LE(AUDIO_AC_TOTAL_LEN),
    0x01, ITF_NUM_AUDIO_STREAMING,

    // Input terminal (USB streaming, 2 channels)
    0x0C, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_INPUT_TERM, AUDIO_ENTITY_INPUT_TERMINAL,
    U16_TO_U8S_LE(AUDIO_TERMINAL_USB_STREAMING), 0x03, 0x02, 0x03, 0x00, 0x00, 0x00,

    // Feature unit (mute control)
    0x0D, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_FEATURE_UNIT, AUDIO_ENTITY_FEATURE_UNIT,
    AUDIO_ENTITY_INPUT_TERMINAL, 0x02, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,

    // Output terminal (generic microphone)
    0x09, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AC_INTERFACE_OUTPUT_TERM, AUDIO_ENTITY_OUTPUT_TERMINAL,
    U16_TO_U8S_LE(AUDIO_TERMINAL_GENERIC_MIC), 0x01, AUDIO_ENTITY_FEATURE_UNIT, 0x00,

    // AudioStreaming interface, alt 0 (zero bandwidth)
    0x09, TUSB_DESC_INTERFACE, ITF_NUM_AUDIO_STREAMING, 0x00, 0x00, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING,
    AUDIO_INT_PROTOCOL_CODE_V1, 0x00,

    // AudioStreaming interface, alt 1 (active)
    0x09, TUSB_DESC_INTERFACE, ITF_NUM_AUDIO_STREAMING, 0x01, 0x01, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_STREAMING,
    AUDIO_INT_PROTOCOL_CODE_V1, 0x00,
    0x07, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_INTERFACE_GENERAL, AUDIO_ENTITY_OUTPUT_TERMINAL, 0x01, 0x01, 0x00,
    0x0B, TUSB_DESC_CS_INTERFACE, AUDIO_CS_AS_FORMAT_TYPE, 0x01, 0x02, 0x04, 24, 0x01,
    U24_TO_U8S_LE(AUDIO_SAMPLE_RATE_HZ),

    // Audio IN endpoint (isochronous asynchronous data endpoint)
    0x09, TUSB_DESC_ENDPOINT, EPNUM_AUDIO_IN, (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS),
    U16_TO_U8S_LE(AUDIO_STREAM_EP_SIZE), AUDIO_POLL_INTERVAL_MS, 0x00, 0x00,
    0x07, TUSB_DESC_CS_ENDPOINT, AUDIO_CS_EP_GENERAL, 0x01, 0x01, 0x01, 0x00
};

_Static_assert(sizeof(desc_configuration) == CONFIG_TOTAL_LEN, "Configuration descriptor size mismatch");

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_configuration; }

static char const *const string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Joseph Pennock",
    "Final SDR",
    "000008", 
    "CDC Interface",
    "SDR Audio Interface"
};

static uint16_t _desc_str[32];
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid; uint8_t chr_count;
    if (index == 0) { memcpy(&_desc_str[1], string_desc_arr[0], 2); chr_count = 1; }
    else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}
