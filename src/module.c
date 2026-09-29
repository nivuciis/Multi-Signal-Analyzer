/*******************************************************************
 * @file module.c
 * @brief Module implementation — CPU/GPIO sampling
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 04/02/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "adc.h"
#include "handles/sigrok_handler.h"
#include "log.h"
#include "module.h"
#include "usb_util.h"

#include <stdint.h>

#include <hardware/structs/sio.h>
#include <hardware/structs/systick.h>
#include <hardware/sync.h>
#include <pico/platform.h>


#define ANA_MAX_MODULES      3
#define ABORT_CHECK_INTERVAL 64u
#define SYSTICK_MASK         0x00FFFFFFu

/*
 * Rate-control strategy switch.
 *
 * Below this many clk_sys cycles per sample, the SysTick-referenced
 * accumulator loop (read csr, subtract, Q8-accumulate, compare) has an
 * overhead comparable to the sample period itself, so its own jitter starts
 * to dominate the measurement. Below the threshold we instead pad the loop
 * body with a fixed, pre-computed count of counted iterations (a
 * subs/bne pair) to hit the target period, trading the timer reference for a
 * cycle-counted busy loop. At/above the threshold, the timer-referenced loop
 * is used, since it self-corrects for the variable overhead of the ADC
 * service call and abort check embedded in it.
 *
 * NOTE: CYCLES_PER_NOP_ITERATION is the cycle cost of one iteration of
 * nop_delay()'s "subs %0,%0,#1; bne 1b" pair on the Cortex-M33. It must be
 * confirmed from the generated assembly (objdump -d) on the actual target,
 * as required by the methodology (Cap.3, Firmware B); 2 is the expected cost
 * for a taken branch with zero wait states, used here as the working value
 * until verified on hardware.
 */
#define FAST_LOOP_THRESHOLD_CYCLES 200u
#define CYCLES_PER_NOP_ITERATION   2u
#define SAMPLE_LOOP_OVERHEAD_CYCLES 12u

static struct ana_module_system *modules[ANA_MAX_MODULES];
static int module_count;

/** Cycles of clk_sys per sample, Q8 fixed point (shared by all modules). */
static uint32_t cycles_per_sample_q8;

void ana_module_gpio_init(struct ana_module_system *config)
{
	log_debug(config->module.name, "Initializing GPIO...");

	for (int i = 0; i < config->module.pin_count; i++) {
		int pin = config->module.pin_base + i;
		gpio_init(pin);
		gpio_set_dir(pin, GPIO_IN);
		gpio_pull_down(pin);
	}

	config->capture.pending = false;
	config->capture.has_complete = false;
	config->trigger.enabled = false;

	if (module_count >= ANA_MAX_MODULES) {
		log_err(config->module.name, "Too many capture modules");
		return;
	}
	modules[module_count] = config;
	module_count++;
}

void ana_module_set_sample_rate(struct ana_module_system *config)
{
	(void)config;
	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();

	cycles_per_sample_q8 =
		(uint32_t)(((uint64_t)clock_get_hz(clk_sys) << 8) / cfg->sample_rate_hz);
}

void ana_module_set_trigger(struct ana_module_system *config, uint8_t gpio,
			    enum ana_trigger_type type)
{
	config->trigger.gpio = gpio;
	config->trigger.type = type;
	config->trigger.enabled = true;
}

void ana_module_clear_trigger(struct ana_module_system *config)
{
	config->trigger.enabled = false;
}

void ana_module_capture_arm(struct ana_module_system *config)
{
	config->capture.pending = true;
	config->capture.has_complete = false;
}

bool ana_module_capture_is_busy(struct ana_module_system *config)
{
	return config->capture.pending && !config->capture.has_complete;
}

void ana_module_capture_abort(struct ana_module_system *config)
{
	config->capture.pending = false;
	config->capture.has_complete = true;
}

static inline bool sampler_aborted(void)
{
	return !ana_usb_is_connected() || ana_usb_abort_requested();
}

/** Poll the trigger GPIO until the condition matches. False on abort. */
static bool trigger_wait(const struct ana_module_trigger *trig)
{
	uint32_t check = 0;
	bool prev = gpio_get(trig->gpio);

	while (true) {
		check++;
		if (check >= ABORT_CHECK_INTERVAL) {
			check = 0;
			ana_adc_capture_service();
			if (sampler_aborted()) {
				return false;
			}
		}

		bool cur = gpio_get(trig->gpio);

		switch (trig->type) {
		case ANA_TRIGGER_LEVEL_LOW:
			if (!cur) {
				return true;
			}
			break;
		case ANA_TRIGGER_LEVEL_HIGH:
			if (cur) {
				return true;
			}
			break;
		case ANA_TRIGGER_EDGE_RISE:
			if (!prev && cur) {
				return true;
			}
			break;
		case ANA_TRIGGER_EDGE_FALL:
			if (prev && !cur) {
				return true;
			}
			break;
		case ANA_TRIGGER_EDGE_BOTH:
			if (prev != cur) {
				return true;
			}
			break;
		default:
			return true;
		}

		prev = cur;
	}
}

static inline void systick_setup(void)
{
	systick_hw->csr = 0x5; /* enable, clocked from clk_sys */
	systick_hw->rvr = SYSTICK_MASK;
	systick_hw->cvr = 0;
}

/** Cycles elapsed since *prev (SysTick counts down, 24-bit wrap). */
static inline uint32_t systick_elapsed(uint32_t *prev)
{
	uint32_t cur = systick_hw->cvr;
	uint32_t delta = (*prev - cur) & SYSTICK_MASK;
	*prev = cur;
	return delta;
}

/** Cycle-counted busy wait: fixed iteration count, no timer reference. */
static inline void __attribute__((always_inline)) nop_delay(uint32_t iterations)
{
	__asm volatile("1: subs %0, %0, #1 \n"
		       "   bne   1b          \n"
		       : "+r"(iterations)
		       :
		       : "cc");
}

/**
 * @brief Run one capture for every armed module.
 *
 * Fills cfg->samples entries of each armed module's buffer with the GPIO
 * word shifted to pin_base and masked to the module's pin mask.
 *
 * @return false if aborted mid-capture (armed state left for the caller to
 * clear via ana_module_capture_abort()).
 *
 * Allocated in SRAM (not flash/XIP) so the sampling loop's timing does not
 * depend on flash cache state (Cap.3, Firmware B: "elimina o jitter de XIP").
 */
static bool __no_inline_not_in_flash_func(cpu_sampler_run)(void)
{
	struct pulseview_sample_config *cfg = ana_sigrok_get_sample_config();

	uint16_t *bufs[ANA_MAX_MODULES];
	uint8_t shifts[ANA_MAX_MODULES];
	uint16_t masks[ANA_MAX_MODULES];
	int n_active = 0;

	for (int i = 0; i < module_count; i++) {
		struct ana_module_system *m = modules[i];

		if (!m->capture.pending || m->capture.buffer == NULL) {
			continue;
		}
		bufs[n_active] = m->capture.buffer;
		shifts[n_active] = m->module.pin_base;
		masks[n_active] = m->module.mask;
		n_active++;
	}

	if (n_active == 0) {
		return true;
	}

	for (int i = 0; i < module_count; i++) {
		struct ana_module_system *m = modules[i];

		if (m->capture.pending && m->trigger.enabled) {
			if (!trigger_wait(&m->trigger)) {
				return false;
			}
		}
	}

	uint32_t samples = cfg->samples;
	uint32_t step_q8 = cycles_per_sample_q8;
	uint32_t cycles_per_sample = step_q8 >> 8;

	/* Open the analog window together with the digital one: the ADC was
	 * only armed at capture start (starting it earlier would overrun its
	 * 8-entry FIFO and slip the round-robin channel phase). */
	ana_adc_capture_kick();

	if (cycles_per_sample < FAST_LOOP_THRESHOLD_CYCLES) {
		/* High rate: cycle-counted busy loop (NOP padding), no timer
		 * reference. The ADC/abort service calls below are skipped
		 * inside the pacing itself, since their overhead alone would
		 * exceed the sample period; abort is still polled between
		 * samples, at the fixed ABORT_CHECK_INTERVAL cadence. */
		uint32_t nop_iters = (cycles_per_sample > SAMPLE_LOOP_OVERHEAD_CYCLES)
					     ? (cycles_per_sample - SAMPLE_LOOP_OVERHEAD_CYCLES) /
						       CYCLES_PER_NOP_ITERATION
					     : 0u;

		for (uint32_t s = 0; s < samples; s++) {
			nop_delay(nop_iters);

			uint32_t word = sio_hw->gpio_in;

			for (int m = 0; m < n_active; m++) {
				bufs[m][s] = (uint16_t)((word >> shifts[m]) & masks[m]);
			}

			if ((s & (ABORT_CHECK_INTERVAL - 1u)) == 0u) {
				ana_adc_capture_service();
				if (sampler_aborted()) {
					return false;
				}
			}
		}
	} else {
		/* Low rate: SysTick-referenced accumulator, self-correcting
		 * for the variable overhead of the ADC service/abort checks
		 * folded into the wait window. */
		uint32_t acc_q8 = 0;
		uint32_t prev;

		systick_setup();
		prev = systick_hw->cvr;

		for (uint32_t s = 0; s < samples; s++) {
			/* Idle wait between samples doubles as the ADC FIFO
			 * drain window: the 8-entry FIFO would overrun while
			 * the CPU is stuck in this loop otherwise. */
			while (acc_q8 < step_q8) {
				ana_adc_capture_service();
				acc_q8 += systick_elapsed(&prev) << 8;
			}
			acc_q8 -= step_q8;

			uint32_t word = sio_hw->gpio_in;

			for (int m = 0; m < n_active; m++) {
				bufs[m][s] = (uint16_t)((word >> shifts[m]) & masks[m]);
			}

			if ((s & (ABORT_CHECK_INTERVAL - 1u)) == 0u) {
				ana_adc_capture_service();
				if (sampler_aborted()) {
					return false;
				}
			}
		}
	}

	for (int i = 0; i < module_count; i++) {
		struct ana_module_system *m = modules[i];

		if (m->capture.pending) {
			m->capture.pending = false;
			m->capture.has_complete = true;
		}
	}

	return true;
}

bool ana_module_capture_wait(struct ana_module_system *config)
{
	if (config->capture.pending) {
		if (!cpu_sampler_run()) {
			return false;
		}
	}

	return config->capture.has_complete;
}

void ana_module_ring_init(struct ana_module_ring *ring, uint16_t *buffer, uint32_t size)
{
	ring->buffer = buffer;
	ring->mask = size - 1u;
	ring->write_idx = 0;
	ring->read_idx = 0;
	ring->overflow = false;
}

uint32_t ana_module_ring_pop(struct ana_module_ring *ring, uint16_t *out, uint32_t max)
{
	__dmb();
	uint32_t w = ring->write_idx;
	uint32_t r = ring->read_idx;
	uint32_t avail = w - r; /* unsigned subtraction: correct across wraparound */
	uint32_t n = (avail < max) ? avail : max;

	for (uint32_t i = 0; i < n; i++) {
		out[i] = ring->buffer[(r + i) & ring->mask];
	}

	__dmb();
	ring->read_idx = r + n;
	return n;
}

/**
 * Allocated in SRAM for the same reason as cpu_sampler_run(): the pacing
 * must not depend on flash/XIP cache state.
 */
void __no_inline_not_in_flash_func(ana_module_dualcore_sample_run)(struct ana_module_system *config,
								     struct ana_module_ring *ring)
{
	uint8_t shift = config->module.pin_base;
	uint16_t mask = config->module.mask;
	uint32_t ring_capacity = ring->mask + 1u;
	uint32_t check = 0;

	if (config->trigger.enabled) {
		if (!trigger_wait(&config->trigger)) {
			return;
		}
	}

	uint32_t step_q8 = cycles_per_sample_q8;
	uint32_t cycles_per_sample = step_q8 >> 8;

	if (cycles_per_sample < FAST_LOOP_THRESHOLD_CYCLES) {
		uint32_t nop_iters = (cycles_per_sample > SAMPLE_LOOP_OVERHEAD_CYCLES)
					     ? (cycles_per_sample - SAMPLE_LOOP_OVERHEAD_CYCLES) /
						       CYCLES_PER_NOP_ITERATION
					     : 0u;

		while (true) {
			nop_delay(nop_iters);

			uint32_t word = sio_hw->gpio_in;
			uint32_t w = ring->write_idx;

			if ((w - ring->read_idx) >= ring_capacity) {
				ring->overflow = true;
			} else {
				ring->buffer[w & ring->mask] =
					(uint16_t)((word >> shift) & mask);
				__dmb();
				ring->write_idx = w + 1u;
			}

			check++;
			if (check >= ABORT_CHECK_INTERVAL) {
				check = 0;
				if (sampler_aborted()) {
					return;
				}
			}
		}
	} else {
		uint32_t acc_q8 = 0;
		uint32_t prev;

		systick_setup();
		prev = systick_hw->cvr;

		while (true) {
			while (acc_q8 < step_q8) {
				acc_q8 += systick_elapsed(&prev) << 8;
			}
			acc_q8 -= step_q8;

			uint32_t word = sio_hw->gpio_in;
			uint32_t w = ring->write_idx;

			if ((w - ring->read_idx) >= ring_capacity) {
				ring->overflow = true;
			} else {
				ring->buffer[w & ring->mask] =
					(uint16_t)((word >> shift) & mask);
				__dmb();
				ring->write_idx = w + 1u;
			}

			check++;
			if (check >= ABORT_CHECK_INTERVAL) {
				check = 0;
				if (sampler_aborted()) {
					return;
				}
			}
		}
	}
}
