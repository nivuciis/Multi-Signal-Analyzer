#ifndef SIGROK_H
#define SIGROK_H

#include "../includes/constraints.h"
#include <stdint.h>

/**
 * @brief Sigrok event types
 *
 */
enum sigrok_evt {
    SIGROK_EVT_SUMP_RESET = 0x00, /**< */
    SIGROK_EVT_SUMP_RUN = 0x01,   /**< */
    SIGROK_EVT_SUMP_ID = 0x02,    /**< */
    SIGROK_EVT_SUMP_DESC = 0x04,  /**< */
    SIGROK_EVT_SUMP_XON = 0x11,   /**< */
    SIGROK_EVT_SUMP_XOFF = 0x13,  /**< */
    SIGROK_EVT_SUMP_DIV = 0x80,   /**< */
    SIGROK_EVT_SUMP_CNT = 0x81,   /**< */
    SIGROK_EVT_SUMP_FLAGS = 0x82, /**< */
};

/**
 * @brief Sigrok protocol structure
 *
 */
struct sigrok_protocol {
    enum sigrok_evt evt;           /**< */
    uint8_t data[MAX_BUFFER_SIZE]; /**< */
    uint8_t dataLength;            /**< */
};

#endif /* SIGROK_H */