/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
 /*******************************************************************
 * @file edge_logic_analyser.h
 *
 * @brief Board-specific definitions for the Edge Logic Analyser
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 1
 * @date 11/02/2026
 *
 * @note: This header may be included by other board headers as "boards/edge_logic_analyser.h"
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/

#ifndef _BOARDS_EDGE_LOGIC_ANALYSER_H
#define _BOARDS_EDGE_LOGIC_ANALYSER_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

/**
 * @brief Board identifier
 *
 */
#define RASPBERRYPI_PICO2

/**
 * @brief RP2350 variant identifier
 *
 */
#define PICO_RP2350A 0

/**
 * @brief UART configuration
 *
 */
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

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
#ifndef PICO_DEFAULT_ADC_VOLTAGE_DIVIDER
#define PICO_DEFAULT_ADC_VOLTAGE_DIVIDER 1
#endif
#ifndef PICO_DEFAULT_ADC_PIN_BASE
#define PICO_DEFAULT_ADC_PIN_BASE 45
#endif
#ifndef PICO_DEFAULT_ADC_PIN_COUNT
#define PICO_DEFAULT_ADC_PIN_COUNT 3
#endif

/**
 * @brief Flash configuration
 * 
 */
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

/**
 * @brief Set the default flash size for the board.
 * 
 * @note: The flash size is set to 4MB (4 * 1024 * 1024)
 */
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (4 * 1024 * 1024))

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif /* _BOARDS_EDGE_LOGIC_ANALYSER_H */