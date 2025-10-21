#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "led_control.h"


int main()
{
    //Initializes led
    led_init();

    // initializes usb
    stdio_init_all();

    while (true) {
        while (!stdio_usb_connected()) {
            led_set_status(LED_STATUS_ERROR);
            sleep_ms(10);
        }

        led_set_status(LED_STATUS_CONNECTED);

        int is_capturing = 0;
    
        while(is_capturing == 0){
            led_set_status(LED_STATUS_CAPTURING); // [cite: 155]
            sleep_ms(10000);
            led_set_status(LED_STATUS_CONNECTED); // Volta ao normal
            sleep_ms(5000);
            led_set_status(LED_STATUS_ERROR);
        }
        sleep_ms(1000);
    }
}
