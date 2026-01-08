/*******************************************************************
 * @file rs485.h
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef RS485_H
#define RS485_H

#include <stdint.h>

#include <pico/types.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>

static PIO pio_rs485 = pio2;
static uint sm_rs485 = 2;
static uint pio_rs485_offset;
static int dma_rs485_chan;

/**
 * @brief GPIO pin for RS485 RX
 * 
 */
#define RS485_GPIO_PIN_RX 31

/**
 * @brief Initialize the RS485 system.
 * 
 */
void ana_rs485_init(void);

#endif /* RS485_H */
