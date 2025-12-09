/** -------------------------------------------------------------
 * @file led_control.c
 * @brief LED control module implementation
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @version   v1.0
 * @date      05/12/2025
 * @copyright
 *  ------------------------------------------------------------*/

#include "blink.pio.h"
#include "led_control.h"

#include <stdio.h>

#include <hardware/gpio.h>
#include <hardware/pio.h>

static PIO pio_instance;
static uint sm;

void init_led(void)
{
	gpio_init(LED_CONNECTED_PIN);
	gpio_set_dir(LED_CONNECTED_PIN, GPIO_OUT);
	gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO);

	gpio_init(LED_ENABLE_BLINK_PIN);
	gpio_set_dir(LED_ENABLE_BLINK_PIN, GPIO_OUT);
	gpio_set_function(LED_ENABLE_BLINK_PIN, GPIO_FUNC_SIO);

	pio_instance = pio0;
	sm = 0;
	uint offset = pio_add_program(pio_instance, &blink_gated_program);
	blink_gated_program_init(pio_instance, sm, offset, LED_CONNECTED_PIN, LED_ENABLE_BLINK_PIN);
	gpio_set_function(LED_ENABLE_BLINK_PIN, GPIO_FUNC_SIO);
	pio_sm_put_blocking(pio_instance, sm, 30000000);
	pio_sm_set_enabled(pio_instance, sm, true);

	set_led_status(LED_STATUS_CONNECTED);
}

void set_led_status(enum led_status new_status)
{
	switch (new_status) {
	case LED_STATUS_ERROR:
		pio_sm_set_enabled(pio_instance, sm, false);
		gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO);
		gpio_put(LED_ENABLE_BLINK_PIN, 0); // Turn the blink off
		gpio_put(LED_CONNECTED_PIN, 0);    // Turn the led off
		break;

	case LED_STATUS_CONNECTED:
		pio_sm_set_enabled(pio_instance, sm, false);
		gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO);
		gpio_put(LED_ENABLE_BLINK_PIN, 0); // turn the blink off
		gpio_put(LED_CONNECTED_PIN, 1);    // Turn the led on
		break;

	case LED_STATUS_READY_TO_CAPTURE:
		pio_gpio_init(pio_instance, LED_CONNECTED_PIN);
		gpio_put(LED_ENABLE_BLINK_PIN, 1); // Starts blink
		pio_sm_set_enabled(pio_instance, sm, true);
		break;

	default:
		// Turn Off all LEDs
		pio_sm_set_enabled(pio_instance, sm, false);
		gpio_put(LED_ENABLE_BLINK_PIN, 0); // turn the blink off
		gpio_put(LED_CONNECTED_PIN, 0);
		break;
	}
}
