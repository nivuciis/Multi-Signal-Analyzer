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

#include "handles/sigrok_handler.h"

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
 * @brief Size of the ping-pong buffer for DMA transfers
 *
 */
#define BUFFER_SIZE 1024

/**
 * @brief Sentinel meaning "this program does not use a jmp pin"
 *
 */
#define ANA_NO_JMP_PIN 0xFF

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
	dma_channel_config instance_cfg; /**< DMA channel configuration */

	/* Chained ping-pong state (gap-free continuous digital capture).
	 * Two DMA channels (instance = A, instance_b = B) auto-chain so the PIO
	 * never stalls; an IRQ recycles each buffer and tracks producer count. */
	uint8_t instance_b;            /**< Second ping-pong DMA channel */
	uint16_t *buf_a;               /**< Ping-pong buffer A (filled by instance) */
	uint16_t *buf_b;               /**< Ping-pong buffer B (filled by instance_b) */
	uint32_t pp_chunk;             /**< Samples per ping-pong buffer */
	uint32_t pp_target;            /**< Buffers needed this capture (0 = unlimited).
					    When the IRQ sees the target reached it halts the
					    PIO SM (and pre-disarms the final chain) so the
					    producer never laps a fully-captured stream. */
	volatile uint32_t pp_produced; /**< Buffers completed by DMA (IRQ) */
	volatile uint32_t pp_consumed; /**< Buffers consumed by Core 1 */
	volatile bool pp_overflow;     /**< Producer lapped consumer → data loss */
};

/**
 * @brief A PIO program together with its generated default-config getter
 *
 */
struct ana_module_program {
	const pio_program_t *program;                          /**< PIO program */
	pio_sm_config (*get_default_cfg_func)(uint8_t offset); /**< Default-config getter */
};

/**
 * @brief Capture programs available to a module.
 *
 * @note Every module provides one free-running program plus one program per
 * trigger type, all indexed by enum ana_trigger_type. Entries left zeroed fall
 * back to @ref ana_module_programs.simple.
 */
struct ana_module_programs {
	struct ana_module_program simple; /**< Free-running capture (no trigger wait) */
	struct ana_module_program
		trigger[ANA_TRIGGER_TYPE_COUNT]; /**< Indexed by enum ana_trigger_type */
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
	struct ana_module_programs programs; /**< Simple + per-trigger-type programs */
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
 * @brief Apply the trigger configuration to the module
 *
 * @param config   Structure referencing the module configuration
 * @param triggers Bitmask of channels-module channels to trigger on
 */
void ana_apply_triggers(struct ana_module_system *config, uint16_t triggers);

/**
 * @brief Load the simple (free-running) PIO program into the module
 *
 *
 * @param config Structure referencing the module configuration
 */
void ana_load_simple_program(struct ana_module_system *config);

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

/**
 * @brief Register a module for chained ping-pong capture.
 *
 * Claims the second DMA channel and records the two ring buffers. Call once
 * after ana_module_dma_init(). The actual channel config is (re)applied in
 * ana_module_pingpong_start() so it always tracks the current PIO SM/DREQ
 * (which changes across ana_module_pio_reload()).
 *
 * @param config Module to register.
 * @param buf_a  First ring buffer (>= chunk samples).
 * @param buf_b  Second ring buffer (>= chunk samples).
 * @param chunk  Samples per buffer.
 */
void ana_module_pingpong_init(struct ana_module_system *config, uint16_t *buf_a, uint16_t *buf_b,
			      uint32_t chunk);

/**
 * @brief Start gap-free chained ping-pong capture. PIO runs continuously.
 */
void ana_module_pingpong_start(struct ana_module_system *config);

/**
 * @brief Stop ping-pong capture: halt PIO and abort both DMA channels.
 */
void ana_module_pingpong_stop(struct ana_module_system *config);

#endif /* MODULE_H */
