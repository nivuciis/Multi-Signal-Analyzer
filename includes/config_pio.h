#ifndef CONFIG_PIO_H
#define CONFIG_PIO_H

#include <stdbool.h>
#include <stdint.h>

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <pico/time.h>
#include <pico/types.h>

/**
 * @brief Configuration structure for PIO programs and DMA
 *
 */
struct ana_config_pio {
	PIO pio;                          /**< PIO instance */
	uint sm;                          /**< State machine index */
	const pio_program_t *pio_program; /**< Pointer to the PIO program */
	uint pio_offset; /**< Offset of the PIO program in the PIO instruction memory */
	pio_sm_config (*get_default_cfg_func)(
		uint offset);             /**< Function to get default SM config */
	uint pin_base;                    /**< Base GPIO pin number */
	uint pin_count;                   /**< Number of GPIO pins */
	uint samples;                     /**< Number of samples to capture */
	uint dma_chan0;                   /**< DMA channel 0 */
	dma_channel_config dma_chan0_cfg; /**< DMA channel 0 configuration */
	void (*dma_callback)(void);       /**< DMA completion callback function */
	uint16_t *dma_buffer;             /**< DMA buffer */
	volatile bool dma_complete;       /**< DMA completion flag */
};

/**
 * @brief Initialize the PIO configuration
 *
 * @param config Structure referencing the PIO program and DMA configuration
 */
void ana_config_pio_init(struct ana_config_pio *config);

/**
 * @brief Initialize the DMA configuration for the PIO program
 *
 * @param config Structure referencing the PIO program and DMA configuration
 */
void ana_config_pio_dma_init(struct ana_config_pio *config);

/**
 * @brief Abort the DMA operation for the PIO program
 *
 * @param config Structure referencing the PIO program and DMA configuration
 */
void ana_config_pio_dma_abort(struct ana_config_pio *config);

/**
 * @brief Start the DMA operation for the PIO program
 *
 * @param config Structure referencing the PIO program and DMA configuration
 */
void ana_config_pio_dma_start(struct ana_config_pio *config);

/**
 * @brief Wait for the DMA operation to complete for the PIO program
 *
 * @param config Structure referencing the PIO program and DMA configuration
 */
void ana_config_pio_dma_wait(struct ana_config_pio *config);

/**
 * @brief Check if the DMA operation is busy for the PIO program
 *
 * @param config Structure referencing the PIO program and DMA configuration
 * @return true  If the DMA is busy
 * @return false Otherwise
 */
bool ana_config_pio_dma_is_busy(struct ana_config_pio *config);

#endif
