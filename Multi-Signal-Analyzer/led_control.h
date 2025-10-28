#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "pico/stdlib.h"

//Defines Gpios for the LED pins

//Setting to the same pin for raspberry pi pico onboard led
//but will change for the PCB board
#define LED_CONNECTED_PIN 25
#define LED_ENABLE_BLINK_PIN 2 

typedef enum
{
    LED_STATUS_ERROR,     
    LED_STATUS_CONNECTED, 
    LED_STATUS_CAPTURING  
} led_status_t;


/**
 * @brief Initialize the LED control system.
 * Initiate with all leds ON
 * 
 */
void led_init(void);

/**
 * @brief 
 *
 * @param new_status 
 */
void led_set_status(led_status_t new_status);

#endif // LED_CONTROL_H