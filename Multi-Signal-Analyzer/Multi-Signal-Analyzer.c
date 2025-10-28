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
        
    }
}
