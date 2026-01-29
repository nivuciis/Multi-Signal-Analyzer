/*******************************************************************
 * @file log.h
 *
 * @brief Logging functions for colored output
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 27/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#ifndef LOG_H
#define LOG_H

/**
 * @brief Log an informational message
 *
 * @param fmt The format string
 * @param ... Additional arguments
 */
void log_inf(char* module, const char *fmt, ...);

/**
 * @brief Log an error message
 *
 * @param fmt The format string
 * @param ... Additional arguments
 */
void log_err(char* module, const char *fmt, ...);

/**
 * @brief Log a warning message
 *
 * @param fmt The format string
 * @param ... Additional arguments
 */
void log_warn(char* module, const char *fmt, ...);

/**
 * @brief Log a debug message
 *
 * @param fmt The format string
 * @param ... Additional arguments
 */
void log_debug(char* module, const char *fmt, ...);

#endif /* LOG_H */