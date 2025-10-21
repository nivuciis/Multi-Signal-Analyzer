#include "led_control.h"
#include "blink.pio.h"
#include "hardware/pio.h"
#include <stdio.h>

void led_init(void)
{
    gpio_init(LED_CONNECTED_PIN);
    gpio_set_dir(LED_CONNECTED_PIN, GPIO_OUT);
    
    // Set initial status to connected
    led_set_status(LED_STATUS_CONNECTED);
}

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq){
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

void led_set_status(led_status_t new_status)
{
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    switch (new_status)
    {
    case LED_STATUS_ERROR: 
        pio_sm_set_enabled(pio, 0, false);
        blink_pin_forever(pio, 0, offset, LED_CONNECTED_PIN, 15);
        break;

    case LED_STATUS_CONNECTED: 
        gpio_put(LED_CONNECTED_PIN, 1);
        break;

    case LED_STATUS_CAPTURING: 
        pio_sm_set_enabled(pio, 0, false);
        blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 4);
        break;
    
    default:
        // Turn Off all LEDs
        gpio_put(LED_CONNECTED_PIN, 0);
        break;
    }
}