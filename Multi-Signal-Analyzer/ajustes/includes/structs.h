#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h>

/**
 * @brief Capture request structure
 *
 */
struct CAPTURE_DATA {
    uint8_t channels[32]; /**< */
    uint8_t loop_count;   /**< */
    uint8_t capture_mode; /**< */
    uint32_t frequency;   /**< */
};

#endif // STRUCTS_H
