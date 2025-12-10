/** -------------------------------------------------------------
 * @file led_control.h
 * @brief LED control module interface
 *
 * @author    Vinicius Rafael Marques de Carvalho <vinicius.carvalho@edge.ufal.br>
 * @version   v1.0
 * @date      05/12/2025
 * @copyright
 *  ------------------------------------------------------------*/

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <pico/stdlib.h>

#define LED_CONNECTED_PIN    25
#define LED_ENABLE_BLINK_PIN 2

/**
 * @brief Led operation states
 */
enum led_status {
	LED_STATUS_ERROR,           /**< An error occurred */
	LED_STATUS_CONNECTED,       /**< Device is connected */
	LED_STATUS_READY_TO_CAPTURE /**< Device is ready to capture data */
};

/**
 * @brief Initialize the LED control system.
 */
void init_led(void);

/**
 * @brief changes the led status
 *
 * @param new_status The target state to set the LED.
 */
void set_led_status(enum led_status new_status);

#endif // LED_CONTROL_H
