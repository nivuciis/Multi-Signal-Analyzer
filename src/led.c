/*******************************************************************
 * @file led.c
 *
 * @brief Led control commands
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.2
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "led.h"
#include "log.h"
#include "macros.h"

#include <stdio.h>

#include <hardware/gpio.h>
#include <pico/time.h>

#define LED_MODULE "led"

#define MSG_INVALID_STATUS "Invalid LED status!\n"

/** Half-period of the capturing blink (matches the old PIO blink rate). */
#define LED_BLINK_HALF_PERIOD_MS 200

static enum led_status current_status = LED_STATUS_OFF;
static struct repeating_timer blink_timer;

/* Toggles the USB LED while capturing; runs from the alarm pool on the core
 * that called ana_led_init() (Core 0), so it keeps blinking while Core 1 is
 * busy inside the CPU sampling loop. */
static bool led_blink_cb(struct repeating_timer *rt)
{
	(void)rt;

	if (current_status == LED_STATUS_CAPTURING) {
		gpio_xor_mask(1u << LED_USB_PIN);
	}
	return true;
}

static void led_reset(void)
{
	gpio_put(LED_BLINK_PIN, 0);
	gpio_put(LED_USB_PIN, 0);
	return;
}

static void led_connected(void)
{
	gpio_put(LED_BLINK_PIN, 0);
	gpio_put(LED_USB_PIN, 1);
	return;
}

static void led_capturing(void)
{
	gpio_put(LED_BLINK_PIN, 1);
	return;
}

void ana_led_init(void)
{
	log_inf(LED_MODULE, "Initializing LED control system...");

	gpio_init(LED_USB_PIN);
	gpio_set_dir(LED_USB_PIN, GPIO_OUT);

	gpio_init(LED_BLINK_PIN);
	gpio_set_dir(LED_BLINK_PIN, GPIO_OUT);

	add_repeating_timer_ms(LED_BLINK_HALF_PERIOD_MS, led_blink_cb, NULL, &blink_timer);

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
	led_op[status].execute();
	current_status = status;
	return;
}

enum led_status ana_led_get_status(void)
{
	return current_status;
}
