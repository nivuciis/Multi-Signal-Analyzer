/*******************************************************************
 * @file led.h
 *
 * @brief Led control for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef LED_H
#define LED_H

/**
 * @brief GPIO pin for USB connected detection
 *
 * @note Sourced from the board header (boards/board_def.h), not hardcoded —
 * a hardcoded GPIO25 here previously collided with RS232 channel 14, which
 * also lives on GPIO25 (PICO_DEFAULT_RS232_PIN_BASE+1).
 */
#define LED_USB_PIN PICO_DEFAULT_LED_PIN

/**
 * @brief GPIO pin to indicate capturing mode
 *
 */
#define LED_BLINK_PIN 2

/**
 * @brief Led operation states
 *
 */
enum led_status {
	LED_STATUS_OFF = 0,   /**< Turn led off */
	LED_STATUS_ERROR,     /**< Indicate an error state at firmware */
	LED_STATUS_CONNECTED, /**< Indicate a connected state with the PC */
	LED_STATUS_CAPTURING, /**< Indicate a capturing state from GPIO by PIO */
	_LED_STATUS_AMOUNT    /**< Represent the number of possible led status */
};

/**
 * @brief LED command structure
 *
 */
typedef struct led_commands {
	void (*execute)(void);
} led_cmd;

/**
 * @brief Initialize the LED control system.
 */
void ana_led_init(void);

/**
 * @brief Changes the led status
 *
 * @param status The new led status to set
 */
void ana_led_set_status(enum led_status status);

/**
 * @brief Get the current led status
 *
 * @return enum led_status The current led status
 */
enum led_status ana_led_get_status(void);

#endif /* LED_H */
