/*******************************************************************
 * @file module.h
 *
 * @brief Base module for the Multi-Signal Analyzer components
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
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

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <pico/error.h>
#include <pico/time.h>
#include <pico/types.h>

/**
 * @brief Size of the capture buffers, in samples
 *
 */
#define BUFFER_SIZE 1024

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
 * @brief CPU capture state for a module.
 *
 * The capture itself is performed by a shared CPU sampler (see module.c):
 * one SIO GPIO read per sample serves every armed module at once, so the
 * per-module state is just the destination buffer and the armed/complete
 * flags.
 */
struct ana_module_capture {
	uint16_t *buffer;           /**< Destination buffer for the next capture */
	volatile bool pending;      /**< Armed, waiting for the CPU sampler to run */
	volatile bool has_complete; /**< Last capture finished */
};

/**
 * @brief CPU trigger condition polled before the sampling loop starts.
 *
 */
struct ana_module_trigger {
	bool enabled;               /**< Trigger armed for the next capture */
	uint8_t gpio;               /**< GPIO polled for the trigger condition */
	enum ana_trigger_type type; /**< Level/edge condition */
};

/**
 * @brief Aggregated state for one capture module
 *
 */
struct ana_module_system {
	struct ana_module_config module;   /**< Module configuration */
	struct ana_module_capture capture; /**< CPU capture state */
	struct ana_module_trigger trigger; /**< CPU trigger condition */
};

/**
 * @brief Initialize the module GPIOs as pulled-down inputs and register the
 * module with the shared CPU sampler.
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_gpio_init(struct ana_module_system *config);

/**
 * @brief Arm the module for the next CPU capture (non-blocking).
 *
 * The actual sampling happens inside ana_module_capture_wait(): the first
 * wait call runs the shared sampler, which fills the buffers of every armed
 * module simultaneously (one GPIO read per sample).
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_capture_arm(struct ana_module_system *config);

/**
 * @brief Run/wait for the CPU capture armed by ana_module_capture_arm().
 *
 * The first waited module drives the shared sampling loop; modules waited
 * afterwards return immediately since their buffer was filled in the same
 * loop.
 *
 * @param config Structure referencing the module configuration
 * @return true  Capture completed
 * @return false Aborted (host '+'/'*' or USB disconnect)
 */
bool ana_module_capture_wait(struct ana_module_system *config);

/**
 * @brief Check whether the module is armed with an unfinished capture.
 *
 * @param config Structure referencing the module configuration
 * @return true  If a capture is pending
 * @return false Otherwise
 */
bool ana_module_capture_is_busy(struct ana_module_system *config);

/**
 * @brief Abort a pending capture (clears the armed state).
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_capture_abort(struct ana_module_system *config);

/**
 * @brief Set the sample rate for the CPU sampler.
 *
 * Computes the SysTick cycle budget per sample from the live sysclk. The
 * pacing is shared by every module (a single sampling loop reads all pins).
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_set_sample_rate(struct ana_module_system *config);

/**
 * @brief Arm a CPU trigger condition for the next capture.
 *
 * The shared sampler polls the GPIO until the condition matches before
 * starting the paced sampling loop.
 *
 * @param config Structure referencing the module configuration
 * @param gpio   GPIO polled for the condition
 * @param type   Level/edge condition
 */
void ana_module_set_trigger(struct ana_module_system *config, uint8_t gpio,
			    enum ana_trigger_type type);

/**
 * @brief Disarm the CPU trigger (captures start immediately).
 *
 * @param config Structure referencing the module configuration
 */
void ana_module_clear_trigger(struct ana_module_system *config);

#endif /* MODULE_H */
