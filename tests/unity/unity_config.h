/**
 * @file    unity_config.h
 * @brief   Custom configuration for Unity unit testing framework.
 * @details Optimizes Unity for embedded C99 meteorological and sensor testing.
 */

#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Standard 32-bit embedded integer width */
#define UNITY_INT_WIDTH         32
#define UNITY_LONG_WIDTH        32
#define UNITY_POINTER_WIDTH     sizeof(void*)

/* Enable floating-point assertions for meteorological calculations */
#define UNITY_INCLUDE_FLOAT
#define UNITY_INCLUDE_DOUBLE
#define UNITY_FLOAT_VERBOSE

/* Enable colorized terminal reporting */
#define UNITY_OUTPUT_COLOR

/* Standard output macro redirection */
#include <stdio.h>
#define UNITY_OUTPUT_CHAR(c)    putchar(c)
#define UNITY_OUTPUT_FLUSH()    fflush(stdout)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_CONFIG_H */
