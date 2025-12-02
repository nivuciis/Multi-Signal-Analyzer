#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

/**
 * @brief Generic command structure
 *
 */
typedef struct commands {
    void (*execute)(uint8_t *payload, uint8_t length);
    void (*erro_command_handler)(int err, void* context);
} Commands;

/**
 * @brief LED command structure
 * 
 */
typedef struct led_commands {
    void (*execute)(void);
} Led_Commands;

#endif /* COMMANDS_H */