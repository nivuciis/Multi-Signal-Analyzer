/*******************************************************************
 * @file macros.h
 *
 * @brief Macros for the Multi-Signal Analyzer project
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 07/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#ifndef MACROS_H
#define MACROS_H

/**
 * @brief Macro to mark an argument as unused to avoid compiler warnings.
 *
 * @param x The unused argument.
 */
#define ARG_UNUSED(x) (void)(x)

/**
 * @brief Checks if a value is within a specified range.
 *
 * @param val Value to check.
 * @param min Minimum allowed value.
 * @param max Maximum allowed value.
 * @param str Error message to print if the value is out of range.
 */
#define CHECK_RANGE(val, min, max, str)                                                            \
	do {                                                                                       \
		if ((val) < (min) || (val) > (max)) {                                              \
			printf(str);                                                               \
			return;                                                                    \
		}                                                                                  \
	} while (0)

#endif /* MACROS_H */
