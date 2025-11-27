#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "led_control.h" 
#include "capture_data.h"  
#include "tusb.h"
#include <bsp/board_api.h>

uint32_t sample_rate = 15000000; 
uint32_t sample_count = 2048*4;
uint16_t  digital_channels_mask = 0x1FFF; 
uint8_t  analog_channels_mask = 0x03; 

#ifdef ANA_LOGIC_ANALYZER
#define DIGITAL_START_PIN 9
#else
#define DIGITAL_START_PIN 0
#endif 

/*
 * @brief Set pins base on masks received from host
 * @param dig_mask: 12 bits (ex: bit 0 = GPIO 8)
 * @param ana_mask: 3 bits 
 */
void configure_pins_from_mask(uint16_t dig_mask, uint8_t ana_mask) {
    
    // Set digital pins 
    for (int i = 0; i < 12; i++) {
        uint pin = DIGITAL_START_PIN + i;
        
        //Check if bit i is set in the digital mask
        if (dig_mask & (1 << i)) {
            gpio_init(pin);
            gpio_set_dir(pin, GPIO_IN);
            gpio_set_pulls(pin, true, false); // pull-up enabled 
        } else {
            gpio_deinit(pin); 
        }
    }
    /*
    // Set analog pins
    for (int i = 0; i < 3; i++) {
        // check if bit i is set in the analog mask
        if (ana_mask & (1 << i)) {
            adc_gpio_init(26 + i); // 26 is the base GPIO for ADC0
        }
    }*/

    digital_channels_mask = dig_mask << DIGITAL_START_PIN; 
    analog_channels_mask = ana_mask;
}

/*
 * @brief Process the received data from USB
 *
 * @param cmd byte 
 */

void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf; 

    uint8_t buf[3];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));

    if (count == 0) return;

    switch (buf[0]){
    
        case 0x01:  
            tud_cdc_write_str("Multi-Signal-Analyzer v1.0\r\n");
            break;

        case 0x10:  
            tud_cdc_write_str("Capture started\r\n");
            tud_cdc_write_flush();
            led_set_status(LED_STATUS_CAPTURING);
            capture_arm_and_send(sample_count, sample_rate);
            return;

        case 0x11:
            if(count < 3) {
                tud_cdc_write_str("ERR: Invalid Set Channel command\r\n");
                break;
            }
            uint16_t combined_config = (buf[2] << 8) | buf[1];
            uint8_t new_analog_mask = combined_config & 0x07;
            uint16_t new_digital_mask = (combined_config >> 3) & 0x0FFF;
            configure_pins_from_mask(new_digital_mask, new_analog_mask);
            tud_cdc_write_str("Channels Set OK\r\n");
            break;

        case 0x12:
            tud_cdc_write_str("Set Triggers command received\r\n");
            break;
        //Set sample rate
        case 0x13:
            tud_cdc_write_str("Set Sample Rate command received\r\n");
            break;
        default:
            tud_cdc_write_str("ERR: Unknown Command\r\n");
            break;
    }


    tud_cdc_write_flush();
}

int main()
{
    //Initializes led
    led_init();

    //Initializes board peripherals
    board_init();
    sleep_ms(1000);
    //initializes capture module
    capture_init();

    //initialize tinyusb
    tusb_init();

    while (true) {
        tud_task(); //tinyusb device task
        //If not connected with usb it doesnt turns the led on 
        while (!tud_connect()) {
            led_set_status(LED_STATUS_ERROR);
            sleep_ms(10);
        }
        led_set_status(LED_STATUS_CONNECTED);
        
    }
}
