#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------+
// CONTROLLER CONFIGURATION
// --------------------------------------------------------------------+
#define CFG_TUSB_MCU                OPT_MCU_RP2040 // RP2350 uses the same USB IP as RP2040

// --------------------------------------------------------------------+
// DEVICE CONFIGURATION
// --------------------------------------------------------------------+
#define CFG_TUD_ENABLED             1

#define CFG_TUSB_RHPORT0_MODE \
    (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#ifndef BOARD_TUD_RHPORT 
#define BOARD_TUD_RHPORT          0
#endif

// Enable the Vendor Class 
#define CFG_TUD_VENDOR              1

#define CFG_TUD_CDC              1

// Buffer sizes (Bulk transfers are usually 64 bytes on USB FS)
#define CFG_TUD_CDC_RX_BUFSIZE   8192*4
#define CFG_TUD_CDC_TX_BUFSIZE   8192*4
#define CFG_TUD_CDC_EP_BUFSIZE   8192*4

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */