/*******************************************************************
 * @file occupancy.c
 * @brief CPU occupancy measurement via the ARM DWT cycle counter
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 29/09/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/
#include "occupancy.h"

#include <stdbool.h>

#include <hardware/structs/m33.h>

#define M33_DEMCR_TRCENA_BITS     (1u << 24)
#define M33_DWT_CTRL_CYCCNTENA_BITS (1u)

static uint32_t total_start_cycles;
static uint32_t total_cycles;
static uint32_t busy_cycles;
static uint32_t busy_start_cycles;
static bool busy_section_open;

static inline void dwt_ensure_enabled(void)
{
	if ((m33_hw->dwt_ctrl & M33_DWT_CTRL_CYCCNTENA_BITS) == 0) {
		m33_hw->demcr |= M33_DEMCR_TRCENA_BITS;
		m33_hw->dwt_cyccnt = 0;
		m33_hw->dwt_ctrl |= M33_DWT_CTRL_CYCCNTENA_BITS;
	}
}

void ana_occupancy_capture_start(void)
{
	dwt_ensure_enabled();
	busy_cycles = 0;
	busy_section_open = false;
	total_start_cycles = m33_hw->dwt_cyccnt;
}

void ana_occupancy_busy_enter(void)
{
	busy_start_cycles = m33_hw->dwt_cyccnt;
	busy_section_open = true;
}

void ana_occupancy_busy_exit(void)
{
	if (busy_section_open) {
		/* Unsigned subtraction: correct even if CYCCNT wrapped
		 * (32-bit free-running counter) between enter and exit. */
		busy_cycles += (m33_hw->dwt_cyccnt - busy_start_cycles);
		busy_section_open = false;
	}
}

void ana_occupancy_capture_end(void)
{
	ana_occupancy_busy_exit(); /* close a section left open by an abort */
	total_cycles = m33_hw->dwt_cyccnt - total_start_cycles;
}

uint32_t ana_occupancy_get_busy_cycles(void)
{
	return busy_cycles;
}

uint32_t ana_occupancy_get_total_cycles(void)
{
	return total_cycles;
}
