#ifndef _BOARD_DEF_H_
#define _BOARD_DEF_H_

/**
 * @brief LED configuration
 *
 * @note Does not have PICO_DEFAULT_LED_PIN.
 */
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 32
#endif

/**
 * @brief CHANNELS configuration
 *
 */
#ifndef PICO_DEFAULT_CHANNELS_PIN_BASE
#define PICO_DEFAULT_CHANNELS_PIN_BASE 8
#endif
#ifndef PICO_DEFAULT_CHANNELS_PIN_COUNT
#define PICO_DEFAULT_CHANNELS_PIN_COUNT 12
#endif

/**
 * @brief CAN configuration
 *
 */
#ifndef PICO_DEFAULT_CAN_PIN_BASE
#define PICO_DEFAULT_CAN_PIN_BASE 35
#endif
#ifndef PICO_DEFAULT_CAN_PIN_COUNT
#define PICO_DEFAULT_CAN_PIN_COUNT 1
#endif

/**
 * @brief RS232 configuration
 *
 */
#ifndef PICO_DEFAULT_RS232_PIN_BASE
#define PICO_DEFAULT_RS232_PIN_BASE 24
#endif
#ifndef PICO_DEFAULT_RS232_PIN_COUNT
#define PICO_DEFAULT_RS232_PIN_COUNT 2
#endif

/**
 * @brief RS482 configuration
 *
 */
#ifndef PICO_DEFAULT_RS485_PIN_BASE
#define PICO_DEFAULT_RS485_PIN_BASE 31
#endif
#ifndef PICO_DEFAULT_RS485_PIN_COUNT
#define PICO_DEFAULT_RS485_PIN_COUNT 1
#endif

/**
 * @brief ADC configuration
 *
 */
#ifndef PICO_DEFAULT_ADC_CHANNEL_1
#define PICO_DEFAULT_ADC_CHANNEL_1 47
#endif
#ifndef PICO_DEFAULT_ADC_CHANNEL_2
#define PICO_DEFAULT_ADC_CHANNEL_2 46
#endif
#ifndef PICO_DEFAULT_ADC_CHANNEL_3
#define PICO_DEFAULT_ADC_CHANNEL_3 45
#endif

#endif /* _BOARD_DEF_H_ */