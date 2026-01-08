/*******************************************************************
 * @file gpios.h
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef GPIOS_H
#define GPIOS_H

#include <stdint.h>

#include <pico/types.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>

static PIO pio_gpios = pio0;
static uint sm_gpios = 1;
static uint pio_gpios_offset;
static int dma_gpios_chan;

/**
 * @brief Number of GPIO pins used.
 * 
 */
#define GPIOS_NUM_PINS 12

/**
 * @brief Starting GPIO pin number.
 * 
 */
#define GPIOS_START_PIN 9

/**
 * @brief Initialize the GPIOs system.
 * 
 */
void ana_gpios_init(void);

#endif /* GPIOS_H */