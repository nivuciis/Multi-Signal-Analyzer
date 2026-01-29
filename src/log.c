/*******************************************************************
 * @file log.c
 * @brief Logging utility implementation
 *
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 27/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/
#include "debug.h"
#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#define ANSI_RESET  "\x1b[0m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"

void log_inf(char *module, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf(ANSI_GREEN "[%s] INFO: ", module);
	vprintf(fmt, args);
	printf(ANSI_RESET "\n");
	va_end(args);
}

void log_err(char *module, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf(ANSI_RED "[%s] ERROR: ", module);
	vprintf(fmt, args);
	printf(ANSI_RESET "\n");
	va_end(args);
}

void log_warn(char *module, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf(ANSI_YELLOW "[%s] WARNING: ", module);
	vprintf(fmt, args);
	printf(ANSI_RESET "\n");
	va_end(args);
}

void log_debug(char *module, const char *fmt, ...)
{
#if DEBUG == 1
	va_list args;
	va_start(args, fmt);
	printf(ANSI_YELLOW "[%s] DEBUG: ", module);
	vprintf(fmt, args);
	printf(ANSI_RESET "\n");
	va_end(args);
#endif
}
