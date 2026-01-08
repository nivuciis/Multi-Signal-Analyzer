/*******************************************************************
 * @file rs232.h
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef RS232_H
#define RS232_H

#include <stdint.h>

#include <pico/types.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>

static PIO pio_rs232 = pio2;
static uint sm_rs232 = 1;
static uint pio_rs232_offset;
static int dma_rs232_chan;


/**
 * @brief GPIO pin for RS232 ROUT2
 *  PERGUNTAR OQ SERIA ESSES ROUTS, TIPO Rx e Tx?
 */
#define RS232_RX_DEVICE2_GPIO_PIN 24

/**
 * @brief GPIO pin for RS232 ROUT1
 * 
 */
#define RS232_RX_DEVICE1_GPIO_PIN 25

/**
 * @brief Initialize the RS232 system.
 * 
 */
void ana_rs232_init(void);

#endif /* RS232_H */
