#ifndef MACROS_H
#define MACROS_H

#include "constraints.h"

/**
 * @brief Macro to mark an argument as unused to avoid compiler warnings.
 *
 * @param x The unused argument.
 */
#define ARG_UNUSED(x) (void)(x)

/**
 * @brief Checks if the frequency is within the allowed range.
 *
 * @param freq Frequency to check.
 */
#define CHECK_FREQUENCY(freq)                                                                                                      \
    do {                                                                                                                           \
        if ((freq) < MIN_FREQUENCY || (freq) > MAX_FREQUENCY) {                                                                    \
            return;                                                                                                                \
        }                                                                                                                          \
    } while (0)

/**
 * @brief Checks if the sample rate is within the allowed range.
 *
 * @param status LED status to check.
 */
#define CHECK_SAMPLE_RATE(rate)                                                                                                    \
    do {                                                                                                                           \
        if ((rate) < MIN_SAMPLE_RATE || (rate) > MAX_SAMPLE_RATE) {                                                                \
            return -ERANGE;                                                                                                        \
        }                                                                                                                          \
    } while (0)

/**
 * @brief Checks if an error condition is met and prints an error message.
 *
 * @param cond Condition to check.
 * @param msg Error message to print.
 */
#define CHECK_ERR_MSG(cond, msg)                                                                                                   \
    do {                                                                                                                           \
        if (cond != 0) {                                                                                                           \
            printf("Error: %s\n", msg);                                                                                            \
            return -ERANGE;                                                                                                        \
        }                                                                                                                          \
    } while (0)

/**
 * @brief Checks if a value is within a specified range.
 *
 * @param val Value to check.
 * @param min Minimum allowed value.
 * @param max Maximum allowed value. 
 */
#define CHECK_RANGE(val, min, max)                                                                                                 \
    do {                                                                                                                           \
        if ((val) < (min) || (val) > (max)) {                                                                                      \
            return -ERANGE;                                                                                                        \
        }                                                                                                                          \
    } while (0)

/**
 * @brief Checks if a value is within a specified range, and calls a fallback function on error.
 * 
 * @param val Value to check.
 * @param min Minimum allowed value.
 * @param max Maximum allowed value.
 * @param fn Fallback function to call on error.
 */
#define CHECK_RANGE_FALLBACK(val, min, max, fn)                                                                                    \
    do {                                                                                                                           \
        if ((val) < (min) || (val) >= (max)) {                                                                                      \
            fn();                                                                                                                  \
            return;                                                                                                                \
        }                                                                                                                          \
    } while (0)

#endif /* MACROS_H */