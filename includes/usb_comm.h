/*******************************************************************
 * @file usb_comm.h
 *
 * @brief USB communication interface for inter-core use.
 *
 * Provides a TX ring buffer so Core 1 (sigrok processing) can send
 * USB data without calling TinyUSB directly. Core 0 owns all TinyUSB
 * calls and drains the ring buffer to the USB CDC interface.
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.1
 * @date 07/05/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef USB_COMM_H
#define USB_COMM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Enqueue data into the USB TX ring buffer.
 *
 * Safe to call from Core 1. Core 0 drains the ring buffer to USB CDC.
 * Spins (blocks) if the ring is full until Core 0 makes space, and
 * returns false immediately if the USB is disconnected.
 *
 * @param buf  Data to send.
 * @param len  Number of bytes.
 * @return true  All bytes were enqueued.
 * @return false USB disconnected before all bytes could be enqueued.
 */
bool ana_usb_write(const uint8_t *buf, uint32_t len);

/**
 * @brief Check whether the USB CDC host is connected.
 *
 * Safe to call from Core 1.
 *
 * @return true  Host is connected.
 * @return false Host is not connected.
 */
bool ana_usb_is_connected(void);

#endif /* USB_COMM_H */
