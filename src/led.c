/*******************************************************************
 * @file led.c
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "blink.pio.h"
#include "led.h"
#include "macros.h"

#include <stdio.h>

#include <hardware/gpio.h>
#include <hardware/pio.h>

#define MSG_INVALID_STATUS "Invalid LED status!\n"

/**
 *  The state machine and PIO instance used for LED control
 *  Fourth state machine of 3rd pio block
 */
static PIO pio_instance = pio2;
static uint sm = 3;
static enum led_status current_status = LED_STATUS_OFF;

static void set_led_pin_to_sio(void)
{
	gpio_set_function(LED_USB_PIN, GPIO_FUNC_SIO);
	return;
}

static void set_led_pin_to_pio(void)
{
	pio_gpio_init(pio_instance, LED_USB_PIN);
	return;
}

static void led_reset(void)
{
	pio_sm_set_enabled(pio_instance, sm, false);
	set_led_pin_to_sio();
	gpio_put(LED_BLINK_PIN, 0);
	gpio_put(LED_USB_PIN, 0);
	return;
}

static void led_connected(void)
{
	pio_sm_set_enabled(pio_instance, sm, false);
	set_led_pin_to_sio();
	gpio_put(LED_BLINK_PIN, 0);
	gpio_put(LED_USB_PIN, 1);
	return;
}

static void led_capturing(void)
{
	set_led_pin_to_pio();
	gpio_put(LED_BLINK_PIN, 1);
	pio_sm_set_enabled(pio_instance, sm, true);
	return;
}

void ana_led_init(void)
{
	gpio_init(LED_USB_PIN);
	gpio_set_dir(LED_USB_PIN, GPIO_OUT);
	gpio_set_function(LED_USB_PIN, GPIO_FUNC_SIO);

	gpio_init(LED_BLINK_PIN);
	gpio_set_dir(LED_BLINK_PIN, GPIO_OUT);
	gpio_set_function(LED_BLINK_PIN, GPIO_FUNC_SIO);
	uint offset = pio_add_program(pio_instance, &blink_gated_program);
	blink_gated_program_init(pio_instance, sm, offset, LED_USB_PIN, LED_BLINK_PIN);
	gpio_set_function(LED_BLINK_PIN, GPIO_FUNC_SIO);

	pio_sm_put_blocking(pio_instance, sm, 30000000);
	pio_sm_set_enabled(pio_instance, sm, true);

	ana_led_set_status(LED_STATUS_CONNECTED);
}

static const led_cmd led_op[_LED_STATUS_AMOUNT] = {
	[LED_STATUS_OFF] = {led_reset},
	[LED_STATUS_ERROR] = {led_reset},
	[LED_STATUS_CONNECTED] = {led_connected},
	[LED_STATUS_CAPTURING] = {led_capturing},
};

void ana_led_set_status(enum led_status status)
{
	CHECK_RANGE(status, LED_STATUS_OFF, _LED_STATUS_AMOUNT, MSG_INVALID_STATUS);
	// led_op[status].execute();PI
	current_status = status;
	return;
}

enum led_status ana_led_get_status(void)
{
	return current_status;
}
