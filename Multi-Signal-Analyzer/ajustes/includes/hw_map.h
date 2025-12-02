#ifndef HW_MAP_H
#define HW_MAP_H

// #define IS_RP2350_A 

#ifdef IS_RP2350_A
#define PIN_BASE_DIGITAL 8
#define DIGITAL_CHANNELS 12

#define PIN_BASE_ANALOG 0
#define ANALOG_CHANNELS 3

#else

#define PIN_BASE_DIGITAL 9
#define DIGITAL_CHANNELS 12

#define PIN_BASE_ANALOG 45
#define ANALOG_CHANNELS 3

#define PIN_RS232_TX 24
#define PIN_RS232_RX 25

#define PIN_RS485 31

#define PIN_CAN 35

#define PIO_BLOCK_DIGITAL_AND_ANALOG pio0
#define PIO_BLOCK_RS232_RS485_CAN pio1

#define SM_DIGITAL_LOC 0
#define SM_ANALOG_LOC 1
#define SM_RS232_LOC 0
#define SM_RS485_LOC 1
#define SM_CAN_LOC 2

#endif // IS_RP2350_A

#endif /* HW_MAP_H */