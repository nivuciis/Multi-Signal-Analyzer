/*******************************************************************
 * @file occupancy.h
 *
 * @brief CPU occupancy measurement via the ARM DWT cycle counter.
 *
 * Used to instrument the M4 metric (Cap.3, Sec. "Métricas e procedimentos
 * experimentais"): the fraction of clk_sys cycles the CPU spends doing
 * productive work during a capture, as opposed to idling/polling for
 * hardware. "Productive" is bracketed explicitly by the caller via
 * ana_occupancy_busy_enter()/ana_occupancy_busy_exit() around whichever
 * code region the firmware variant considers CPU-bound work (see the call
 * sites in sigrok_handler.c for what each firmware brackets).
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 29/09/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef OCCUPANCY_H
#define OCCUPANCY_H

#include <stdint.h>

/**
 * @brief Arm DWT->CYCCNT (idempotent) and reset the per-capture counters.
 *
 * Call once at the start of a capture command, before any
 * ana_occupancy_busy_enter() call.
 */
void ana_occupancy_capture_start(void);

/**
 * @brief Mark the start of a CPU-bound "productive work" section.
 *
 * Cycles between a matching ana_occupancy_busy_enter()/busy_exit() pair
 * accumulate into the busy-cycle total. Sections may be entered/exited
 * repeatedly within one capture; the totals accumulate across all of them.
 */
void ana_occupancy_busy_enter(void);

/**
 * @brief Mark the end of a CPU-bound "productive work" section.
 *
 * Safe to call without a preceding ana_occupancy_busy_enter() in the same
 * capture (no-op if no section is open).
 */
void ana_occupancy_busy_exit(void);

/**
 * @brief Stop the total-cycles timer for the current capture.
 *
 * Call once when the capture command completes (before the done/abort
 * marker is sent), regardless of whether it finished normally or was
 * aborted. If a busy section was left open (e.g. abort mid-section), it is
 * closed here first so its cycles are still counted.
 */
void ana_occupancy_capture_end(void);

/** @return Accumulated busy cycles for the last completed capture. */
uint32_t ana_occupancy_get_busy_cycles(void);

/** @return Total elapsed clk_sys cycles for the last completed capture. */
uint32_t ana_occupancy_get_total_cycles(void);

#endif /* OCCUPANCY_H */
