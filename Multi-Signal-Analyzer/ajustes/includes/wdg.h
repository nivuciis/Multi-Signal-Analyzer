#ifndef WDG_H
#define WDG_H

#include "hardware/watchdog.h"
#include <stdio.h>

/**
 * @brief Initialize the watchdog timer with a specified timeout.
 * 
 * @param timeout_ms - Timeout in milliseconds.
 */
void watchdog_init();

/**
 * @brief Check if the last reboot was caused by the watchdog.
 * 
 */
void check_watchdog_reboot();

/**
 * @brief Feed (reset) the watchdog timer to prevent a reboot.
 * 
 */
void feed_watchdog();



#endif /* WDG_H */