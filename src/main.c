 /*******************************************************************
 * @file main.c
 *
 * @brief Main file for the workstation manager application.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

 #include <pico/stdio_usb.h>
 #include <pico/time.h>

 #include "led.h"

 static bool is_usb_connected = false;

 static void sync_led_with_usb_connection() {
    while (!stdio_usb_connected()) {
        ana_led_set_status(LED_STATUS_OFF);
        is_usb_connected = false;
        sleep_ms(10);
        return;
    }
    ana_led_set_status(LED_STATUS_CONNECTED);
    is_usb_connected = true;
}

int main(){
    stdio_init_all();
    ana_led_init();

    while (1) {
        sync_led_with_usb_connection();
    }

    return 0;
}