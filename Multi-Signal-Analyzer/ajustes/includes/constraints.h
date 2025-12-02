#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

/**
 * @brief Frequency constraints for the system
 *
 */
#define MAX_FREQUENCY 120000000 /** 120 MHz */
#define MIN_FREQUENCY 1000      /** 1 kHz */

/**
 * @brief Sample rate constraints for the system
 *
 */
#define MAX_SAMPLE_RATE 1000000 /** 1 MSPS */
#define MIN_SAMPLE_RATE 1000    /** 1 kSPS */

/**
 * @brief Channel constraints for the system
 *
 */
#define MAX_CHANNELS 32       /** maximum number of digital channels */
#define MAX_ANALOG_CHANNELS 3 /** maximum number of analog channels */

/**
 * @brief Buffer size constraints for the system
 *
 */
#define MAX_BUFFER_SIZE 512 /** maximum buffer size */

/**
 * @brief Command FIFO size constraints for the system
 *
 */
#define MAX_COMMANDS_FIFO 10 /** maximum number of commands in the FIFO */

#endif // CONSTRAINTS_H