/*******************************************************************
 * @file sigrok_handler.c
 *
 * @brief Handles the Sigrok protocol communication over USB CDC.
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author Vinicius Rafael Marques de Carvalho (vinicius.carvalho@edge.ufal.br)
 * @version 0.1
 * @date 15/01/2026
 *
 * @copyright Copyright (c) 2026
 *
 *******************************************************************/


#include "sigrok_handler.h"
#include "capture_data.h"
#include "led.h"
#include <tusb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> 

#include <hardware/vreg.h>
#include <hardware/clocks.h>

static uint32_t sample_rate = 5000;
static uint32_t num_samples = 1024;

static char cmd_str[32];
static int cmd_str_pointer = 0;

extern uint32_t digital_capture_buffer[];
extern uint8_t analog_capture_buffer[];

/**
 * @brief Send a response string over USB CDC
 * 
 * @param str Response string to send
 */
static void ana_send_response(const char* str) {
    if (tud_cdc_connected()) {
        tud_cdc_write(str, strlen(str));
        tud_cdc_write_flush();
    }
}

/**
 * @brief Send mixed digital and analog signal data over USB CDC
 * 
 * @note Data is sent in packets of 64 bytes
 */
static void ana_send_data_buffers(void) {
    uint8_t packet[64];
    uint8_t id = 0;
    
    for (uint32_t i = 0; i < num_samples; i++) {

        uint16_t d_val = (uint16_t)(digital_capture_buffer[i] >> 16);
        packet[id++] = 0x80 | (d_val & 0x7F);
        packet[id++] = 0x80 | ((d_val >> 7) & 0x7F);
        packet[id++] = 0x80 | ((d_val >> 14) & 0x03);

        packet[id++] = 0x80 | (analog_capture_buffer[i*3 + 0] >> 1);
        packet[id++] = 0x80 | (analog_capture_buffer[i*3 + 1] >> 1);
        packet[id++] = 0x80 | (analog_capture_buffer[i*3 + 2] >> 1);

        if (id >= 60) {
            tud_cdc_write(packet, id);
            id = 0;
        }
    }
    if (id > 0) {
        tud_cdc_write(packet, id);
    }
    tud_cdc_write_flush();
}

void sigrok_init(void) {
    cmd_str_pointer = 0;
    memset(cmd_str, 0, sizeof(cmd_str));
}


void sigrok_process_byte(uint8_t received_command) {
    char response[64];
    response[0] = '\0'; 
    
    if (received_command == '*') {
        sigrok_init();
        ana_led_set_status(LED_STATUS_OFF);
        return;
    }

    if (received_command == '\r' || received_command == '\n') {
        cmd_str[cmd_str_pointer] = '\0';
        
        strcpy(response, "*"); 

        switch (cmd_str[0]) {
            case IDENTIFY_CMD: 
                tud_cdc_write_str("SRPICO,A031D16,02");
                tud_cdc_write_flush();
                ana_led_set_status(LED_STATUS_CONNECTED);
                break;

            case SET_SAMPLE_RATE_CMD: 
                sample_rate = atol(&cmd_str[1]);
                if (sample_rate < 5000){
                    sample_rate = 5000;
                }else if (sample_rate >= 200000000){
                    #ifdef ENABLE_OVERCLOCKING
                    vreg_set_voltage(VREG_VOLTAGE_1_25); 
                    sleep_ms(1);  
                    set_sys_clock_khz(250000, true);
                    #endif
                    sample_rate = 200000000;
                }
                break;

            case SET_SAMPLE_LIMIT_CMD: 
                num_samples = atol(&cmd_str[1]);
                if (num_samples > CAPTURE_BUFFER_SIZE) {
                    num_samples = CAPTURE_BUFFER_SIZE;
                }
                break;

            case GET_ANALOG_SCALE_CMD: 
                snprintf(response, sizeof(response), "25700x0");
                break;

            case ENABLE_ANALOG_CHANNEL_CMD: 
                break;

            case ENABLE_DIGITAL_CHANNEL_CMD: 
                break;

            case FIXED_CAPTURE_CMD: 
                response[0] = 0;
                ana_led_set_status(LED_STATUS_CAPTURING);
                ana_capture_data(num_samples, sample_rate, NULL);
                ana_send_data_buffers();
                ana_led_set_status(LED_STATUS_CONNECTED);
                break;

            default:
                break;
        }

        if (response[0] != 0) {
            ana_send_response(response);
        }

        cmd_str_pointer = 0;
    } 
    else {
        if (cmd_str_pointer < 31) {
            cmd_str[cmd_str_pointer] = (char)received_command;
            cmd_str_pointer += 1;
        } else {
            cmd_str_pointer = 0; 
        }
    }
}