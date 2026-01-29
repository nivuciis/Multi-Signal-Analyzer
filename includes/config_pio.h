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

#define ANA_CONFIG_MODULE_NAME_MAX_LEN 8

/**
 * @brief Configuration structure for each module
 *
 */
struct ana_config_modulo {
	char name[ANA_CONFIG_MODULE_NAME_MAX_LEN]; /**< Name of the module */
	uint pin_base;                             /**< Base GPIO pin number */
	uint pin_count;                            /**< Number of GPIO pins */
	uint samples;                              /**< Number of samples */
	uint32_t sample_rate_hz;                      /**< Sample rate in Hz */
};

/**
 * @brief Configuration structure for DMA
 *
 */
struct ana_config_dma {
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
struct ana_config_pio {
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
struct ana_config_system {
	struct ana_config_modulo module; /**< Module configuration */
	struct ana_config_pio pio;       /**< PIO configuration */
	struct ana_config_dma dma;       /**< DMA configuration */
};

/**
 * @brief Initialize the PIO configuration
 *
 * @param config Structure referencing the PIO program and DMA configuration
 */
void ana_config_pio_init(struct ana_config_system *config);

/**
 * @brief Initialize the DMA configuration for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_dma_init(struct ana_config_system *config);

/**
 * @brief Abort the DMA operation for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_dma_abort(struct ana_config_system *config);

/**
 * @brief Start the DMA operation for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_dma_start(struct ana_config_system *config);

/**
 * @brief Wait for the DMA operation to complete for the PIO program
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_dma_wait(struct ana_config_system *config);

/**
 * @brief Check if the DMA operation is busy for the PIO program
 *
 * @param config Structure referencing the module configuration
 * @return true  If the DMA is busy
 * @return false Otherwise
 */
bool ana_config_pio_dma_is_busy(struct ana_config_system *config);

/**
 * @brief Get data from PIO program with DMA.
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_get_data(struct ana_config_system *config);

/**
 * @brief Get data from PIO program with DMA (version 2).
 *
 */
void ana_config_pio_get_data_v2(struct ana_config_system *config);

/**
 * @brief Print PIO program data.
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_print_data(struct ana_config_system *config);

/**
 * @brief Test PIO direct functionality.
 *
 * @param config Structure referencing the module configuration
 */
void ana_config_pio_test_pio_direct(struct ana_config_system *config);

/**
 * @brief Get buffer of PIO program data.
 *
 * @param config Structure referencing the module configuration
 */
uint16_t *ana_config_pio_get_buffer(struct ana_config_system *config);

void ana_config_pio_diagnose(struct ana_config_system *config);

#endif
