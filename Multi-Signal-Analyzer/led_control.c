#include "led_control.h"
#include "blink.pio.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include <stdio.h>

static PIO pio_instance;
static uint sm;

// Devolve o controle do pino do LED para a CPU (SIO)
static void set_led_pin_to_sio(void) {
    gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO);
}
// Devolve o controle do pino do LED para o PIO
static void set_led_pin_to_pio(void) {
    // pio_gpio_init é a função que define o pino para o PIO
    pio_gpio_init(pio_instance, LED_CONNECTED_PIN);
}


void led_init(void)
{
    gpio_init(LED_CONNECTED_PIN);
    gpio_set_dir(LED_CONNECTED_PIN, GPIO_OUT);
    gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO);

    gpio_init(LED_ENABLE_BLINK_PIN);
    gpio_set_dir(LED_ENABLE_BLINK_PIN, GPIO_OUT);
    gpio_set_function(LED_ENABLE_BLINK_PIN, GPIO_FUNC_SIO);

    //PIO inits
    pio_instance = pio0;
    sm = 0;
    uint offset = pio_add_program(pio_instance, &blink_gated_program);
    blink_gated_program_init(pio_instance, sm, offset, LED_CONNECTED_PIN, LED_ENABLE_BLINK_PIN);
    gpio_set_function(LED_ENABLE_BLINK_PIN, GPIO_FUNC_SIO);
    pio_sm_put_blocking(pio_instance, sm, 30000000);
    pio_sm_set_enabled(pio_instance, sm, true);
    
    // Set initial status to connected
    led_set_status(LED_STATUS_CONNECTED);
}

void led_set_status(led_status_t new_status)
{
    switch (new_status)
    {
    case LED_STATUS_ERROR: 
        pio_sm_set_enabled(pio_instance, sm, false);
        set_led_pin_to_sio();
        gpio_put(LED_ENABLE_BLINK_PIN, 0);//Turn the blink off
        gpio_put(LED_CONNECTED_PIN, 0); //Turn the led off
        break;

    case LED_STATUS_CONNECTED: 
        pio_sm_set_enabled(pio_instance, sm, false);
        set_led_pin_to_sio();
        gpio_put(LED_ENABLE_BLINK_PIN, 0); // turn the blink off
        gpio_put(LED_CONNECTED_PIN, 1);//Turn the led on 
        break;

    case LED_STATUS_CAPTURING: 
        set_led_pin_to_pio();
        gpio_put(LED_ENABLE_BLINK_PIN, 1); //Starts blink
        pio_sm_set_enabled(pio_instance, sm, true);
        break;
    
    default:
        // Turn Off all LEDs
        pio_sm_set_enabled(pio_instance, sm, false);
        set_led_pin_to_sio();
        gpio_put(LED_ENABLE_BLINK_PIN, 0); // turn the blink off
        gpio_put(LED_CONNECTED_PIN, 0);
        break;
    }
}