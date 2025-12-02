#include "hardware/gpio.h"
#include "hardware/pio.h"

#include "../includes/commands.h"
#include "../includes/led.h"
#include "../includes/macros.h"

#include "blink.pio.h"
/**
 *  The state machine and PIO instance used for LED control
 *  Fourth state machine of second pio block 
 */
static PIO pio_instance = pio2;
static uint sm = 3;
static enum LED_OP current_status = LED_STATUS_OFF;

static void set_led_pin_to_sio(void) { gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO); }

static void set_led_pin_to_pio(void) { pio_gpio_init(pio_instance, LED_CONNECTED_PIN); }

static void led_reset(void) {
    pio_sm_set_enabled(pio_instance, sm, false);
    set_led_pin_to_sio();
    gpio_put(LED_ENABLE_BLINK_PIN, 0);
    gpio_put(LED_CONNECTED_PIN, 0);
}

static void led_connected(void) {
    pio_sm_set_enabled(pio_instance, sm, false);
    set_led_pin_to_sio();
    gpio_put(LED_ENABLE_BLINK_PIN, 0);
    gpio_put(LED_CONNECTED_PIN, 1);
}

static void led_capturing(void) {
    set_led_pin_to_pio();
    gpio_put(LED_ENABLE_BLINK_PIN, 1);
    pio_sm_set_enabled(pio_instance, sm, true);
}

void led_init(void) {
    gpio_init(LED_CONNECTED_PIN);
    gpio_set_dir(LED_CONNECTED_PIN, GPIO_OUT);
    gpio_set_function(LED_CONNECTED_PIN, GPIO_FUNC_SIO);

    gpio_init(LED_ENABLE_BLINK_PIN);
    gpio_set_dir(LED_ENABLE_BLINK_PIN, GPIO_OUT);
    gpio_set_function(LED_ENABLE_BLINK_PIN, GPIO_FUNC_SIO);

    uint offset = pio_add_program(pio_instance, &blink_gated_program);
    blink_gated_program_init(pio_instance, sm, offset, LED_CONNECTED_PIN, LED_ENABLE_BLINK_PIN);
    gpio_set_function(LED_ENABLE_BLINK_PIN, GPIO_FUNC_SIO);

    pio_sm_put_blocking(pio_instance, sm, 30000000);
    pio_sm_set_enabled(pio_instance, sm, true);

    // Set initial status to connected
    led_set_status(LED_STATUS_CONNECTED);
}

static const Led_Commands led_op[_LED_STATUS_AMOUNT] = {[LED_STATUS_OFF] = {led_reset},
                                                       [LED_STATUS_ERROR] = {led_reset},
                                                       [LED_STATUS_CONNECTED] = {led_connected},
                                                       [LED_STATUS_CAPTURING] = {led_capturing}};

void led_set_status(enum LED_OP status) {
    CHECK_RANGE_FALLBACK(status, LED_STATUS_OFF, _LED_STATUS_AMOUNT, led_reset);
    led_op[status].execute();
    current_status = status;
}

enum LED_OP led_get_status(void){
    return current_status;
}
