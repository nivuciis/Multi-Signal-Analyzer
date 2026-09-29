#include "handles/handles_internal.h"
#include "occupancy.h"

#include <stdio.h>

/**
 * @brief Reports busy/total DWT cycles for the last completed capture.
 *
 * Response format: "<busy_cycles>/<total_cycles>" (both decimal, ASCII),
 * followed by the usual '*' ack (see ana_sigrok_handle_process_byte()).
 * Host-side scripts (benchmarks/m4_cpu_occupancy.py) divide the two to get
 * the M4 occupancy fraction (Cap.3, Sec. "Métricas e procedimentos
 * experimentais").
 */
void handle_get_cpu_occupancy(void)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%lu/%lu", (unsigned long)ana_occupancy_get_busy_cycles(),
		 (unsigned long)ana_occupancy_get_total_cycles());

	ana_send_response(buf);
}
