/*******************************************************************
 * @file main.c
 *
 * @brief Main file for the workstation manager application.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#include "adc.h"
#include "can.h"
#include "cmd.h"
#include "gpios.h"
#include "led.h"
#include "log.h"
#include "pwm.h"
#include "rs232.h"
#include "rs485.h"

#include <stdbool.h>
#include <stdio.h>

#include <pico/multicore.h>
#include <pico/stdio_usb.h>
#include <pico/time.h>
#include <pico/types.h>

static bool is_usb_connected = false;

static void sync_led_with_usb_connection()
{
	is_usb_connected = stdio_usb_connected();
	ana_led_set_status((is_usb_connected) ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
}

static int get_cmd_by_serial(void)
{
	int cmd = getchar_timeout_us(1000);
	if (cmd != PICO_ERROR_TIMEOUT) {
		return cmd;
	}
	return PICO_ERROR_TIMEOUT;
}

int main()
{
	enum ana_cmd_code cmd;
	uint level = 1;
	int dt = 1;

	stdio_init_all();
	multicore_reset_core1();
	multicore_launch_core1(ana_pwm_generate);

	sleep_ms(500);

	ana_adc_init();
	ana_adc_set_clkdiv(10.0f);
	ana_can_init();
	ana_gpios_init();
	ana_led_init();
	ana_rs232_init();
	ana_rs485_init();
	ana_pwm_init();
	ana_cmd_table();

	while (1) {
		sync_led_with_usb_connection();

		if (is_usb_connected) {
			cmd = get_cmd_by_serial();
			if (cmd >= 0 && cmd < _ANA_CMD_AMOUNT) {
				ana_cmd_process(cmd);
			}else {
				log_debug("main", "Command ignored [cmd]: %d", cmd);
			}
			
		}
		sleep_ms(30);
	}

	return 0;
}
