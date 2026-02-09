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
	char *name;              /**< Name of the module */
	uint pin_base;           /**< Base GPIO pin number */
	uint pin_count;          /**< Number of GPIO pins */
	uint samples;            /**< Number of samples */
	uint16_t mask;           /**< Pin mask to sump protocol*/
	uint32_t sample_rate_hz; /**< Sample rate in Hz */
};

/**
 * @brief Configuration structure for DMA
 *
 */
struct ana_module_dma {
	uint instance;                   /**< DMA channel number */
	dma_channel_config instance_cfg; /**< DMA channel configuration */
	void (*callback)(void);          /**< DMA callback function */
	uint16_t *dma_buffer;            /**< DMA buffer pointer */
	volatile bool has_complete;      /**< DMA completion flag */
};

/**
 * @brief Configuration structure for PIO
 *
 */
struct ana_module_pio {
	PIO instance;                     /**< PIO instance */
	uint sm;                          /**< State machine number */
	const pio_program_t *pio_program; /**< Pointer to the PIO program */
	uint pio_offset;                  /**< Offset of the PIO program */
	pio_sm_config (*get_default_cfg_func)(
		uint offset); /**< Function to get default state machine configuration */
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

#endif /* MODULE_H */
