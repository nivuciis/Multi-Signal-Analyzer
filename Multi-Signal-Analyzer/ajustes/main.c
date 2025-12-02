#include "pico/stdlib.h"

#include "includes/led.h"
#include "includes/pc_comunication.h"
#include "includes/wdg.h"
#include "includes/cfg.h"

#include <pico/stdio_usb.h>
#include <stdio.h>
#include <string.h>

#include "includes/wdg.h"

bool is_alive = false;

void keep_led_connected_while_waiting_usb() {
    while (!stdio_usb_connected()) {
        led_set_status(LED_STATUS_OFF);
        is_alive = false;
        feed_watchdog();
        sleep_ms(10);
        return;
    }
    led_set_status(LED_STATUS_CONNECTED);
    is_alive = true;
}

uint8_t get_cmd_by_serial(void) {
    int cmd = getchar_timeout_us(10000);
    if (cmd != PICO_ERROR_TIMEOUT) {
        return (uint8_t) cmd;
    }
    return CMD_NONE;
}

int main() {
    uint8_t cmd;

    stdio_init_all();

    led_init();

    watchdog_init();

    cfg_init();

    capture_init(8, 1000);

    while (true) {
        cmd = 0;
        keep_led_connected_while_waiting_usb();

        if (is_alive) {
            cmd = get_cmd_by_serial();
            process_cmd(cmd, NULL, 4);
        }

        feed_watchdog();
        process_cmd(CMD_ARM_CAPTURE, NULL, 0);
    }
}