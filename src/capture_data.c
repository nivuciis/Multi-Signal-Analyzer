/** -------------------------------------------------------------
 * @file capture_data.c
 * @brief Capture data module implementation
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @author   João Matheus Nascimento Dias <joao.dias@edge.ufal.br>
 * @version   0.2
 * @date      28/01/2026
 * @copyright  Copyright (c) 2026
 *  ------------------------------------------------------------*/

#include "capture_data.h"
#include "handles/sigrok_handler.h"
#include "led.h"
#include "module.h"
#include "usb_util.h"

#include <stdint.h>

#include <hardware/adc.h>

#define ADC_MAX_RATE         500000
#define DIGITAL_CHANNEL_SIZE 12
#define ANALOG_CHANNEL_SIZE  3

int ana_capture_init(struct ana_module_system *config)
{
	if (ana_module_capture_is_busy(config)) {
		ana_module_capture_abort(config);
	}

	return PICO_OK;
}

int ana_capture_data_get_analog_channels_count(uint8_t analog_mask)
{
	int enabled_analog_channel_count = 0;
	for (int i = 0; i < ANALOG_CHANNEL_SIZE; i++) {
		if (analog_mask & (1 << i)) {
			enabled_analog_channel_count++;
		}
	}
	return enabled_analog_channel_count;
}

void ana_capture_data_start(struct ana_module_system *config)
{
	ana_led_set_status(LED_STATUS_CAPTURING);
	ana_capture_init(config);
	ana_module_capture_arm(config);
}

bool ana_capture_data_wait(struct ana_module_system *config)
{
	if (!ana_usb_is_connected() || ana_usb_abort_requested()) {
		ana_module_capture_abort(config);
		return false;
	}

	if (!ana_module_capture_wait(config)) {
		ana_module_capture_abort(config);
		return false;
	}

	return true;
}
