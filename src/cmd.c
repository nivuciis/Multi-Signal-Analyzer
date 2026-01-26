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

#include "adc.h"
#include "can.h"
#include "cmd.h"
#include "gpios.h"
#include "led.h"
#include "rs232.h"
#include "rs485.h"

#include <stdio.h>

#include <pico/time.h>

static enum ana_cmd_code last_cmd = ANA_CMD_NONE;

typedef struct {
	void (*execute)(void);
} cmd;

static void cmd_none(void)
{
}

static void adc_print(int samples, int channel, double *adc_values)
{
	printf("ADC Channel %d Values:\n", channel);
	for (int i = 0; i < samples; i++) {
		printf("Sample %d: %.3f mV\n", i + 1, adc_values[i]);
	}
}

static void cmd_adc_test(void)
{
	int samples = 3;

	double adc_values_channel1[samples];
	double adc_values_channel2[samples];
	double adc_values_channel3[samples];

	ana_adc_read_multiple_channels(adc_values_channel1, adc_values_channel2,
				       adc_values_channel3, samples);

	adc_print(samples, 1, adc_values_channel1);
	adc_print(samples, 2, adc_values_channel2);
	adc_print(samples, 3, adc_values_channel3);
}

static void _test(struct ana_config_system *config)
{
	printf("Starting %s capture\n", config->module.name);
	ana_config_pio_get_data(config);

	printf("%s capture finished\n", config->module.name);
	ana_config_pio_print_data(config);
}

static void cmd_can_test(void)
{
	_test(ana_can_get_config());
}

static void cmd_gpios_test(void)
{
	_test(ana_gpios_get_config());
}

static void cmd_led_test(void)
{
	printf("EXEC LED OFF\n");
	ana_led_set_status(LED_STATUS_OFF);
	sleep_ms(5000);

	printf("EXEC LED CONNECTED\n");
	ana_led_set_status(LED_STATUS_CONNECTED);
	sleep_ms(5000);

	printf("EXEC LED ERROR\n");
	ana_led_set_status(LED_STATUS_ERROR);
	sleep_ms(5000);

	printf("EXEC LED CAPTURING\n");
	ana_led_set_status(LED_STATUS_CAPTURING);
	sleep_ms(5000);

	printf("TEST LED finished\n");
}

static void cmd_rs232_test(void)
{
	_test(ana_rs232_get_config());
}

static void cmd_rs485_test(void)
{
	_test(ana_rs485_get_config());
}

static const cmd cmd_list[_ANA_CMD_AMOUNT] = {
	[ANA_CMD_NONE] = {cmd_none},        [ANA_CMD_ADC] = {cmd_adc_test},
	[ANA_CMD_CAN] = {cmd_can_test},     [ANA_CMD_GPIOS] = {cmd_gpios_test},
	[ANA_CMD_LED] = {cmd_led_test},     [ANA_CMD_RS232] = {cmd_rs232_test},
	[ANA_CMD_RS485] = {cmd_rs485_test},
};

void ana_cmd_process(enum ana_cmd_code code_cmd)
{
	if (code_cmd <= ANA_CMD_NONE || code_cmd >= _ANA_CMD_AMOUNT) {
		printf("Invalid command code: %d\n", code_cmd);
		return;
	}

	cmd_list[code_cmd].execute();
	last_cmd = code_cmd;
}
