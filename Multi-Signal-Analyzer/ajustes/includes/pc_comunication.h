#ifndef PC_COMUNICATION_H
#define PC_COMUNICATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief
 *
 */
enum pc_com_status {
    PC_COM_STATUS_IDLE = 0,   /**< */
    PC_COM_STATUS_RECEIVING,  /**< */
    PC_COM_STATUS_PROCESSING, /**< */
    PC_COM_STATUS_SENDING,    /**< */
    PC_COM_STATUS_ERROR,      /**< */
    _PC_COM_AMOUNT            /**< */
};

/**
 * @brief
 *
 */
enum pc_com_cmd {
    CMD_GET_INFO = 0X00,     /**< */
    CMD_GET_STATUS = 0X01,   /**< */
    CMD_SET_CONFIG = 0X02,   /**< */
    CMD_ARM_CAPTURE = 0X03,  /**< */
    CMD_STOP_CAPTURE = 0X04, /**< */
    CMD_NONE = 0x05,         /**< */
    _CMD_AMOUNT,             /**< */
};

/**
 * @brief
 *
 * @param buffer
 * @param len
 * @return true
 * @return false
 */
bool read_serial(uint8_t *buffer, size_t len);

/**
 * @brief
 *
 * @param channels
 */
void configure_digital_pins(uint8_t *channels);

/**
 * @brief
 *
 * @param cmd
 */
void process_cmd(uint8_t cmd, uint8_t *payload, uint8_t length);

/**
 * @brief Get the current PC communication status
 *
 * @return enum pc_com_status
 */
enum pc_com_status pc_get_status();

#endif /* PC_COMUNICATION_H */