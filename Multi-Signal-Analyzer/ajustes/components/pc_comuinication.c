#include "../includes/cfg.h"
#include "../includes/commands.h"
#include "../includes/led.h"
#include "../includes/macros.h"
#include "../includes/pc_comunication.h"
#include "../includes/util.h"
#include "../includes/versions.h"
#include "../includes/wdg.h"

#include "../includes/hw_map.h"
#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <pico/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BYTES_MAX_LEN 4

static enum pc_com_status current_status = PC_COM_STATUS_IDLE;
static uint8_t bytes_received[BYTES_MAX_LEN] = {0};

/******************************************************************************
 *      SET OF COMMANDS TO BE USED AT PC COMMUNICATION BY PROCESS_CMD FUNCTION
 */

static void erro_handler(int err, void *context);
static void cmd_get_info(uint8_t *payload, uint8_t length);
static void cmd_get_status(uint8_t *payload, uint8_t length);
static void cmd_set_pin_config(uint8_t *payload, uint8_t length);
static void cmd_arm_capture(uint8_t *payload, uint8_t length);
static void cmd_stop_capture(uint8_t *payload, uint8_t length);
static void cmd_none(uint8_t *payload, uint8_t length);

static void fallback_process_cmd();

/******************************************************************************/

void configure_digital_pins(uint8_t *channels) {

    // TODO: Quando definir o protocolo dos canais aplicar as mascaras para pegar os pinos;
    // Ex: os 2 bytes mais significativos byte = digital, o proximo byte = analogico, o proximo byte = rs_can, etc
    uint16_t digital_channels_mask = channels[1] | (channels[0] << 8);
    uint8_t analog_channels_mask = channels[2];
    uint8_t rs_can_channels_mask = channels[3];

    uint32_t pin_mask = 0xffffffff;

    // for (int bit = 0; bit < DIGITAL_CHANNELS; ++bit) {
    //     if (digital_channels_mask & (1u << bit)) {
    //         pin_mask |= (1u << (PIN_BASE_DIGITAL + bit));
    //     }
    // }

    pin_mask &= ~((1u << LED_CONNECTED_PIN) | (1u << LED_ENABLE_BLINK_PIN));

    if (pin_mask == 0) {
        printf("configure_digital_pins: no digital pins enabled\n");
    } else {
        gpio_init_mask(pin_mask);
        gpio_set_dir_in_masked(pin_mask);

        for (int gp = 0; gp < 32; ++gp) {
            if (pin_mask & (1u << gp)) {
                gpio_set_pulls(gp, false, false);
            }
        }
    }

    printf("DIGITAL CHANNELS MASK: 0x%04X\n", digital_channels_mask);
    printf("ANALOG CHANNELS MASK: 0x%02X\n", analog_channels_mask);
    printf("RS/CAN CHANNELS MASK: 0x%02X\n", rs_can_channels_mask);

    feed_watchdog();
}

// TODO: Implement the actual command functions using sigrok protocol
static void fallback_process_cmd() { printf("ERR: Command out of range\n"); }

static void erro_handler(int err, void *context) { printf("ERR: Command execution error %d\n", err); }

static void cmd_get_info(uint8_t *payload, uint8_t length) {
    ARG_UNUSED(payload);
    ARG_UNUSED(length);

    printf("Project: %s\n", PROJECT_NAME);
    printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    printf("Hardware Version: %s\n", HARDWARE_VERSION);
    printf("PC Communication Status: %d\n", pc_get_status());
    cmd_get_status(NULL, 0);
}

static void cmd_get_status(uint8_t *payload, uint8_t length) {
    ARG_UNUSED(payload);
    ARG_UNUSED(length);

    printf("Led status: %d\n", led_get_status());
    printf("Command status: %d\n", pc_get_status());
}

static void cmd_set_pin_config(uint8_t *payload, uint8_t length) {
    ARG_UNUSED(payload);

    if (length > BYTES_MAX_LEN) {
        printf("ERR: Payload length exceeds maximum allowed (%d bytes)\n", BYTES_MAX_LEN);
        return;
    }

    memset(bytes_received, 0, BYTES_MAX_LEN * sizeof(uint8_t));

    if (read_serial(bytes_received, length)) {
        configure_digital_pins(bytes_received);
    } else {
        printf("ERR: Timeout on PIN CONFIG payload\n");
    }
}

bool teste = false;
static void cmd_arm_capture(uint8_t *payload, uint8_t length) {
    uint32_t sample_count = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | (payload[3]);
    capture_init(sample_count, length);
    cfg_start_capture();
}

static void cmd_stop_capture(uint8_t *payload, uint8_t length) {
    ARG_UNUSED(payload);
    ARG_UNUSED(length);
}

static void cmd_none(uint8_t *payload, uint8_t length) {
    ARG_UNUSED(payload);
    ARG_UNUSED(length);
}

static Commands pc_commands[_CMD_AMOUNT] = {
    [CMD_GET_INFO] = {cmd_get_info, erro_handler},         [CMD_GET_STATUS] = {cmd_get_status, erro_handler},
    [CMD_SET_CONFIG] = {cmd_set_pin_config, erro_handler}, [CMD_ARM_CAPTURE] = {cmd_arm_capture, erro_handler},
    [CMD_STOP_CAPTURE] = {cmd_stop_capture, erro_handler}, [CMD_NONE] = {cmd_none, erro_handler}};

void process_cmd(uint8_t cmd, uint8_t *payload, uint8_t length) {
    CHECK_RANGE_FALLBACK(cmd, CMD_GET_INFO, _CMD_AMOUNT, fallback_process_cmd);
    pc_commands[cmd].execute(payload, length);
    current_status = cmd;
}

enum pc_com_status pc_get_status() { return current_status; }