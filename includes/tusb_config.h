
/*******************************************************************
 * @file tusb_config.h
 *
 * @brief TinyUSB configuration file
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.1
 * @date 09/01/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines the USB IP for TinyUSB device stack
 *
 * @note RP2350 uses the same USB IP as RP2040
 *
 */
#define CFG_TUSB_MCU OPT_MCU_RP2040

/**
 * @brief Enables TinyUSB device stack.
 *
 */
#define CFG_TUD_ENABLED 1

/**
 * @brief Set the USB as device or host.
 *
 */
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

/**
 * @brief Set the USB port (default = 0).
 *
 */
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

/**
 * @brief Number of USB Vendor Class to enable.
 *
 */
#define CFG_TUD_VENDOR 0

/**
 * @brief Number of CDC (Serial) interfaces to enable.
 *
 */
#define CFG_TUD_CDC 1

/**
 * @brief CDC FIFO buffer size configuration.
 * Defines the size of the ring buffers used for serial data transfer.
 *
 * @note The value (8192 * 4) results in a 32KB buffer, ensuring high throughput
 * but consuming significant RAM.
 */
#define CFG_TUD_CDC_RX_BUFSIZE (8192 * 4) /**< Host to Device */
#define CFG_TUD_CDC_TX_BUFSIZE (8192 * 4) /**< Device to Host */
#define CFG_TUD_CDC_EP_BUFSIZE (8192 * 4) /**< Endpoint limit*/

/**
 * @brief Max packet size for Endpoint 0 (Control Endpoint).
 *
 * @note Used for USB enumeration and control transfers.
 * @note Standard size for Full/High Speed is 64 bytes.
 */
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
