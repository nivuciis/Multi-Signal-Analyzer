#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "led_control.h" 
#include "capture_data.h"  

uint32_t sample_rate = 10000000; 
uint32_t sample_count = 1024;
uint16_t  digital_channels_mask = 0x1FFF; 
uint8_t  analog_channels_mask = 0x03; 


/*
 * @brief Read specified bytes in serial line.
 * @param buffer Pointer to destination buffer to read
 * @param len number of bytes to read
 * @return true if sucessfully read the data from buffer 
 */
bool read_serial_blocking(uint8_t* buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int c = getchar_timeout_us(1000000); 
        if (c == PICO_ERROR_TIMEOUT) {
            return false; //Timeout exception
        }
        buffer[i] = (uint8_t)c;
    }
    return true;
}

//Function to configure Pins based on a channels list 
void configure_digital_pins(uint8_t channels[13]) {
    
    uint32_t pin_mask = 0;
    
    for (int ch = 0; ch <= 12; ch++) {
        if (channels[ch] != 0) {
            #ifdef ANA_LOGIC_ANALYZER
                pin_mask |= (1u << (ch + 8)); // I use this because the PCB first channel is GPIO8 and it goes up to GPIO20
            #else
                pin_mask |= (1u << ch);
            #endif
        }
    }

    if (pin_mask == 0) {
        return; // No pins to configure
    }

    // Initialize all pins in the mask
    gpio_init_mask(pin_mask);
    
    // Set direction for all pins in the mask to INPUT
    gpio_set_dir_in_masked(pin_mask);

    // Set pulls
    for (int ch = 0; ch <= 12; ch++) {
        if (channels[ch] != 0) {
            int shifted_channel;
            
            #ifdef ANA_LOGIC_ANALYZER
                shifted_channel = ch + 8; 
                // I use this because the PCB first channel is GPIO8 and it goes up to GPIO20
            #else
                shifted_channel = ch;
            #endif
            
            // Enable pull-down
            gpio_set_pulls(shifted_channel, false, true);
        }
    }
}




/*
 * @brief Process the received data from USB
 *
 * @param cmd byte 
 */
bool process_command(uint8_t cmd) {
    bool is_capturing = false;
    uint8_t payload[16];
    switch (cmd) {
        case 0x01: // CMD_GET_INFO
            // Send back an identification command
            printf("Multi-Signal-Analyzer v1.0\n");
            break;

        case 0x10: // CMD_ARM_CAPTURE
            
            led_set_status(LED_STATUS_CAPTURING); //Start the capture
            sleep_ms(10);
            //Configure pins based on channel selection  
            uint8_t channel[13];
            for (int ch = 0; ch <= 12; ch++) {
                // Checks if bit ch is set on array mask
                if (digital_channels_mask & (1u << ch)) {
                    channel[ch] = 1; // Set channel if so
                } else {
                    channel[ch] = 0; 
                }
            }
            configure_digital_pins(channel);
            is_capturing = true;

            capture_arm_and_send(sample_count, sample_rate);
         /*
            for (int ch = 0; ch <= 12; ch++) {
                printf("Channel %d, value %d\n", ch, channel[ch]);
            }*/
            led_set_status(LED_STATUS_CONNECTED);
            is_capturing = false;
            break;

        case 0x11: // Set Sample Rate
            break;
        /*
         * Reads 3 Bytes: 
         *     2 Bytes to receive information on digital channels (UINT16_T)
         *     1 byte to receive information on analog channels (UINT8_T)
         */
        case 0x12: //Set Channels
            if (read_serial_blocking(payload, 3)) { //Returns true if correctly read 3 bytes from serial  
                    // [xxxxxxxx]<<8 = [xxxxxxxx00000000], ++ [xxxxxxxx] = [XXXXXXXX](MSB)[xxxxxxxx](LSB) 
                    digital_channels_mask = (payload[1]<<8) | payload[0] ;
                    analog_channels_mask = payload[2];
                    printf("OK: Channels set D=0x%04X A=0x%02X\n", digital_channels_mask, analog_channels_mask);

                } else {
                    printf("ERR: Timeout on CHANNELS payload\n");
                }
                
            break;
        case 0x13: //Set Triggers
            break;



        default:
            printf("ERR: Unknown command 0x%02X\n", cmd);
            break;
    }
    return is_capturing;
}

int main()
{
    //Initializes led
    led_init();

    // initializes usb
    stdio_init_all();
    //initializes capture module
    capture_init();

    while (true) {
        //If not connected with usb it doesnt turns the led on 
        while (!stdio_usb_connected()) {
            led_set_status(LED_STATUS_ERROR);
            sleep_ms(10);
        }

        led_set_status(LED_STATUS_CONNECTED);
        int cmd = getchar_timeout_us(10000);
        if (cmd != PICO_ERROR_TIMEOUT) {
            uint8_t is_capturing = process_command((uint8_t)cmd);
        }
    }
}
