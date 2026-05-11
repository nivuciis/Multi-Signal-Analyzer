/*******************************************************************
 * @file module.h
 *
 * @brief Base module for the Multi-Signal Analyzer components
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 04/02/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef MODULE_H
#define MODULE_H

#include "sigrok_handler.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hardware/address_mapped.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/error.h>
#include <pico/time.h>
#include <pico/types.h>

/**
 * @brief Configuration structure for each module
 *
 */
struct ana_module_config {
	uint16_t mask;     /**< Pin mask to sump protocol*/
	uint8_t pin_base;  /**< Base GPIO pin number */
	uint8_t pin_count; /**< Number of GPIO pins */
	char *name;        /**< Name of the module */
};

/**
 * @brief Configuration structure for DMA
 *
 */
struct ana_module_dma {
	uint16_t *dma_buffer;            /**< DMA buffer pointer */
	uint8_t instance;                /**< DMA channel number */
	volatile bool has_complete;      /**< DMA completion flag */
	void (*callback)(void);          /**< DMA callback function */
	dma_channel_config instance_cfg; /**< DMA channel configuration */
};

/**
 * @brief Configuration structure for PIO
 *
 */
struct ana_module_pio {
	PIO instance;                     /**< PIO instance */
	uint8_t sm;                       /**< State machine number */
	uint8_t pio_offset;               /**< Offset of the PIO program */
	uint8_t jmp_pin;                  /**< Pin used for conditional jumps in the PIO program */
	const pio_program_t *pio_program; /**< Pointer to the PIO program */
	pio_sm_config (*get_default_cfg_func)(
		uint8_t offset); /**< Function to get default state machine configuration */
};

/**
 * @brief Configuration structure for PIO programs and DMA module
 *
 */
struct ana_module_system {
	struct ana_module_config module; /**< Module configuration */
	struct ana_module_pio pio;       /**< PIO configuration */
	struct ana_module_dma dma;       /**< DMA configuration */
};

/**
 * @brief Initialize the PIO configuration
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_pio_init(struct ana_module_system *config);

/**
 * @brief Initialize the DMA configuration
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_dma_init(struct ana_module_system *config);

/**
 * @brief Abort the DMA operation for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_pio_dma_abort(struct ana_module_system *config);

/**
 * @brief Start the DMA operation for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_pio_dma_start(struct ana_module_system *config);

/**
 * @brief Wait for the DMA operation to complete for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_pio_dma_wait(struct ana_module_system *config);

/**
 * @brief Check if the DMA operation is busy for the PIO program
 *
 * @param config Structure referencing the module configuration
 * @return true  If the DMA is busy
 * @return false Otherwise
 */
bool ana_module_pio_dma_is_busy(struct ana_module_system *config);

/**
 * @brief Set the sample rate for the module
 *
 * @param config Structure referencing the module configuration
 * @param sample_rate_hz Sample rate in Hz
 */
void ana_module_set_sample_rate(struct ana_module_system *config);

/**
 * @brief Reload the PIO program with a new program and optional jmp_pin.
 *
 * @note Disables the SM, removes the old program, loads the new one and
 * re-initialises the state machine.  Call before each capture when the
 * trigger type or channel may have changed.
 *
 * @param config          Structure referencing the module configuration
 * @param new_program     New PIO program to load
 * @param new_cfg_func    Default-config getter for the new program
 * @param jmp_pin         GPIO used by jmp pin (0xFF = not used)
 */
void ana_module_pio_reload(struct ana_module_system *config, const pio_program_t *new_program,
			   pio_sm_config (*new_cfg_func)(uint8_t offset), uint8_t jmp_pin);
#endif /* MODULE_H */
