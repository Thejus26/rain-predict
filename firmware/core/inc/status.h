/**
 * @file    status.h
 * @brief   Universal system status codes and error definitions.
 * @details Conforms to MISRA-C and zero-dynamic-allocation embedded standards.
 */

#ifndef STATUS_H
#define STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Standard system error and return status enumeration.
 */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERR_BUSY,
    STATUS_ERR_TIMEOUT,
    STATUS_ERR_I2C,
    STATUS_ERR_UART_BUS,
    STATUS_ERR_CRC_MISMATCH,
    STATUS_ERR_SENSOR_NO_RESPONSE,
    STATUS_ERR_INVALID_PARAM,
    STATUS_ERR_NULL_PTR,
    STATUS_ERR_NOT_INITIALIZED,
    STATUS_ERR_INVALID_STATE,
    STATUS_ERR_OVERFLOW,
    STATUS_ERR_UNDERFLOW,
    STATUS_ERR_OUT_OF_RANGE,
    STATUS_ERR_HARDWARE,
    STATUS_ERR_DATA_CORRUPT,
    STATUS_ERR_MODBUS_EXCEPTION
} status_t;

/* Standard Compatibility Aliases */
#define STATUS_ERROR_HARDWARE           STATUS_ERR_HARDWARE
#define STATUS_ERROR_DATA_CORRUPT       STATUS_ERR_DATA_CORRUPT
#define STATUS_ERROR_NULL_POINTER       STATUS_ERR_NULL_PTR
#define STATUS_ERROR_INVALID_PARAM      STATUS_ERR_INVALID_PARAM
#define STATUS_ERROR_TIMEOUT            STATUS_ERR_TIMEOUT
#define STATUS_ERROR_CRC                STATUS_ERR_CRC_MISMATCH
#define STATUS_ERROR_INVALID_FRAME      STATUS_ERR_DATA_CORRUPT
#define STATUS_ERROR_BUFFER_OVERFLOW    STATUS_ERR_OVERFLOW
#define STATUS_ERROR_BUFFER_TOO_SMALL   STATUS_ERR_UNDERFLOW
#define STATUS_ERROR_MODBUS_EXCEPTION   STATUS_ERR_MODBUS_EXCEPTION

#ifdef __cplusplus
}
#endif

#endif /* STATUS_H */
