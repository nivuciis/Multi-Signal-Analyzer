/*******************************************************************
 * @file cmd.c
 *
 * @brief Commands control to teste firmware features
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @version 0.1
 * @date 12/01/2026
 *
 * @copyright Copyright (c) 2025
 *
 *******************************************************************/

#include "cmd.h"

#include <stdio.h>

typedef struct cmd_commands {
	void (*execute)(void);
} cmd;

static void cmd_adc_test(void)
{
}

static void cmd_can_test(void)
{
}

static void cmd_gpios_test(void)
{
}

static void cmd_led_test(void)
{
}

static void cmd_rs232_test(void)
{
}

static void cmd_rs484_test(void)
{
}

static const cmd cmd_list[_ANA_CMD_AMOUNT] = {
	[ANA_CMD_ADC] = cmd_adc_test,     [ANA_CMD_CAN] = cmd_can_test,
	[ANA_CMD_GPIOS] = cmd_gpios_test, [ANA_CMD_LED] = cmd_led_test,
	[ANA_CMD_RS232] = cmd_rs232_test, [ANA_CMD_RS484] = cmd_rs484_test,
};

void ana_cmd_process(enum ana_cmd_code cmd_code)
{
	if (cmd_code < ANA_CMD_NONE && cmd_code > _ANA_CMD_AMOUNT) {
		printf("Invalid command code: %d\n", cmd_code);
	}

	cmd_list[cmd_code].execute();
	return;
}
