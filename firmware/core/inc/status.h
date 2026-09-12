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
    STATUS_ERR_OVERFLOW,
    STATUS_ERR_UNDERFLOW,
    STATUS_ERR_OUT_OF_RANGE
} status_t;

#ifdef __cplusplus
}
#endif

#endif /* STATUS_H */
