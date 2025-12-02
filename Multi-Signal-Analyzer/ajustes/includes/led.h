#ifndef LED_H
#define LED_H

#define LED_CONNECTED_PIN 25
#define LED_ENABLE_BLINK_PIN 2

/**
 * @brief LED operation states
 *
 */
enum LED_OP {
    LED_STATUS_OFF = 0,   /**< */
    LED_STATUS_ERROR,     /**< */
    LED_STATUS_CONNECTED, /**< */
    LED_STATUS_CAPTURING, /**< */
    _LED_STATUS_AMOUNT     /**< */
};

/**
 * @brief Initialize the LED control system.
 */
void led_init(void);

/**
 * @brief changes the led status
 *
 * @param new_status The new LED status to set
 */
void led_set_status(enum LED_OP status);

/**
 * @brief Get the current LED status
 * 
 * @return enum LED_OP  the current LED status
 */
enum LED_OP led_get_status(void);

#endif /* LED_H */