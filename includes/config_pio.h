/*******************************************************************
 * @file config_pio.h
 *
 * @brief Configuration for PIO programs
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 08/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#ifndef CONFIG_PIO_H
#define CONFIG_PIO_H

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

#include <pico/types.h>
#include <stdint.h>

/**
 * @brief Function pointer type for getting default PIO SM configuration
 *
 */
typedef pio_sm_config (*get_default_config)(uint offset);

/**
 * @brief Initialize configuration settings
 * 
 */
void ana_config_pio_init(void);

/**
 * @brief Abort any running PIO or DMA processes to avoid crashing the system
 *
 * @param pio Base pio instance
 * @param sm State machine to abort
 * @param dma_chan DMA channel to abort
 */
void ana_config_pio_initial_abort(PIO pio, uint sm, uint dma_chan);

/**
 * @brief Specific configuration for PIO and SM
 *
 * @param get_cfg Callback to get default configuration
 * @param offset This is offset where the program is loaded in PIO memory
 * @param pin_base Which pin the PIO will start reading data from ISR (GPIOs)
 * @param prog PIO program struct
 * @param sample_rate Desired sample rate for the capture
 *
 * @return pio_sm_config Configured PIO SM configuration (returned by value).
 */
pio_sm_config ana_config_pio_pio(get_default_config get_cfg, uint offset, uint pin_base, struct pio_program prog, uint32_t sample_rate);

/**
 * @brief
 *
 * @param pio Base pio instance
 * @param sm State machine to configure
 * @param dma_chan DMA channel to configure
 * @param sample_count Number of samples to capture
 */
void ana_config_pio_dma(PIO pio, uint sm, uint dma_chan, uint32_t sample_count, uint32_t *buffer);


#endif /* CONFIG_PIO_H */