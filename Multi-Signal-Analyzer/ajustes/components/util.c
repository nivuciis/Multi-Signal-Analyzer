#include "../includes/util.h"
#include "includes/wdg.h"
#include <pico/stdio.h>
#include <stdint.h>
#include <stdio.h>

void send_capture_data(const uint32_t *buffer, uint32_t num_samples) {
    fwrite(buffer, sizeof(uint32_t), num_samples, stdout);
    fflush(stdout);
}

void print_buffer(uint32_t* buffer) {
    uint32_t raw = buffer[0];

    printf("BITS: ");
    for(int i = 0; i < 32; i++) {
        if (i % 8 == 0 && i != 0) {
            printf("_");
        }

        printf("%d", (raw>>i) & 0x01);
    }
    printf("\n");
}

bool read_serial(uint8_t *buffer, size_t len) {
    for (uint8_t i = 0; i < len; i++) {
        int byte_read = getchar_timeout_us(1000000);

        if (byte_read == PICO_ERROR_TIMEOUT) {
            printf("Timeout on byte %d\n", i);
            return false;
        }

        buffer[i] = (uint8_t) byte_read;
        feed_watchdog();
    }
    return true;
}
