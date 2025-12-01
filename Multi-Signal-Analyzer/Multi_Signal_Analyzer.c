/** -------------------------------------------------------------
 * @file multi_signal_analyzer.c
 * @brief Main application file for Multi-Signal Analyzer
 *
 * @author    Vinicius Rafael Marques de Carvalho vinicius.carvalho@edge.ufal.br
 * @version   v1.0
 * @date      28/11/2025
 * @copyright
 *  ------------------------------------------------------------*/

#include "capture_data.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "led_control.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include <stdio.h>

#include <bsp/board_api.h>

static uint32_t sample_rate = 15000000;
static uint32_t sample_count = 2048 * 4;
static uint16_t digital_channels_mask = 0x1FFF;
static uint8_t analog_channels_mask = 0x03;

static uint8_t new_analog_mask;
static uint16_t new_digital_mask;
static uint16_t combined_config;

#ifdef ANA_LOGIC_ANALYZER
#define DIGITAL_START_PIN 9
#else
#define DIGITAL_START_PIN 0
#endif

/**
 * @brief Set pins base on masks received from host
 * @param dig_mask: 12 bits (ex: bit 0 = GPIO 8)
 * @param ana_mask: 3 bits
 */
void configure_pins_from_mask(uint16_t dig_mask, uint8_t ana_mask)
{

	// Set digital pins
	for (int i = 0; i < 12; i++) {
		uint pin = DIGITAL_START_PIN + i;

		if (dig_mask & (1 << i)) {
			gpio_init(pin);
			gpio_set_dir(pin, GPIO_IN);
			gpio_set_pulls(pin, true, false); 
		} else {
			gpio_deinit(pin);
		}
	}
	digital_channels_mask = dig_mask << DIGITAL_START_PIN;
	analog_channels_mask = ana_mask;
}

/**
 * @brief Process the received data from USB
 *
 * @param cmd byte
 */

void tud_cdc_rx_cb(uint8_t itf)
{
	(void)itf;

	uint8_t buf[3];
	uint32_t count = tud_cdc_read(buf, sizeof(buf));

	if (count == 0) {
		return;
	}

	switch (buf[0]) {

	case 0x01:
		tud_cdc_write_str("Multi-Signal-Analyzer v1.0\r\n");
		break;

	case 0x10:
		tud_cdc_write_str("Capture started\r\n");
		tud_cdc_write_flush();
		led_set_status(LED_STATUS_CAPTURING);
		capture_data(sample_count, sample_rate);
		break;

	case 0x11:
		if (count < 3) {
			tud_cdc_write_str("ERR: Invalid Set Channel command\r\n");
			break;
		}
		combined_config = (buf[2] << 8) | buf[1];
		new_analog_mask = combined_config & 0x07;
		new_digital_mask = (combined_config >> 3) & 0x0FFF;
		configure_pins_from_mask(new_digital_mask, new_analog_mask);
		tud_cdc_write_str("Channels Set OK\r\n");
		break;

	case 0x12:
		tud_cdc_write_str("Set Triggers command received\r\n");
		break;
	case 0x13:
		tud_cdc_write_str("Set Sample Rate command received\r\n");
		break;
	default:
		tud_cdc_write_str("ERR: Unknown Command\r\n");
		break;
	}

	tud_cdc_write_flush();
}

int main()
{
	led_init();
	board_init();
	capture_init();
	tusb_init();
	sleep_ms(1000);

	while (!tud_cdc_connected()) {
			led_set_status(LED_STATUS_ERROR);
			tud_task(); 
		}
	while (true) {
		tud_task();
		led_set_status(LED_STATUS_CONNECTED);
	}
}
