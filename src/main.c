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

#include <pico/stdio_usb.h>
#include <pico/time.h>

static bool is_usb_connected = false;

static void sync_led_with_usb_connection()
{
	is_usb_connected = stdio_usb_connected();
	ana_led_set_status((is_usb_connected) ? LED_STATUS_CONNECTED : LED_STATUS_OFF);
}

static enum ana_cmd_code get_cmd_by_serial(void)
{
	int cmd = getchar_timeout_us(1000);
	if (cmd != PICO_ERROR_TIMEOUT) {
		return cmd;
	}
	return ANA_CMD_NONE;
}

static void _cmd_table()
{
	printf("\n \tMulti-Signal Analyzer - Command Table\n");
	log_inf("main", "0x00 - Reserved");
	log_inf("main", "0x01 - ADC Test");
	log_inf("main", "0x02 - CAN Test");
	log_inf("main", "0x03 - GPIOS Test");
	log_inf("main", "0x04 - LED Test");
	log_inf("main", "0x05 - RS232 Test");
	log_inf("main", "0x06 - RS485 Test\n");
	log_inf("main", "0x07 - PWM Test");
}

int main()
{
	enum ana_cmd_code cmd;

	stdio_init_all();

	sleep_ms(500);

	ana_adc_init();
	ana_adc_set_clkdiv(10.0f);
	ana_can_init();
	ana_gpios_init();
	ana_led_init();
	ana_rs232_init();
	ana_rs485_init();
	ana_pwm_init();

	_cmd_table();

	while (1) {
		sync_led_with_usb_connection();

		if (is_usb_connected) {
			cmd = get_cmd_by_serial();
			if (cmd != 0) {
				ana_cmd_process(cmd);
			}
		}
		sleep_ms(30);
	}

	return 0;
}
