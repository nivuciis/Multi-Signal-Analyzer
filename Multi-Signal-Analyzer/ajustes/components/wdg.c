#include <stdbool.h>
#include "../includes/wdg.h"

#define WHATCHDOG_TIMEOUT_MS 50000 /** Watchdog timeout in milliseconds */

void watchdog_init() {
    watchdog_enable(WHATCHDOG_TIMEOUT_MS, true);
}

void check_watchdog_reboot() {
    if (watchdog_enable_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
    }
}

void feed_watchdog() {
    watchdog_update();
}
