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

#include <stdint.h>
#include <stdio.h>

/* Standard 32-bit embedded integer width */
#define UNITY_INT_WIDTH         32
#define UNITY_LONG_WIDTH        32
#define UNITY_POINTER_WIDTH     sizeof(void*)

/* 64-bit assertion macro extensions */
#ifndef TEST_ASSERT_EQUAL_HEX64
#define TEST_ASSERT_EQUAL_HEX64(expected, actual)                              \
    do {                                                                       \
        TEST_ASSERT_EQUAL_HEX32((uint32_t)(((uint64_t)(expected)) >> 32),     \
                                (uint32_t)(((uint64_t)(actual)) >> 32));       \
        TEST_ASSERT_EQUAL_HEX32((uint32_t)((uint64_t)(expected)),              \
                                (uint32_t)((uint64_t)(actual)));               \
    } while (0)
#endif

#ifndef TEST_ASSERT_EQUAL_UINT64
#define TEST_ASSERT_EQUAL_UINT64(expected, actual)                             \
    do {                                                                       \
        TEST_ASSERT_EQUAL_UINT32((uint32_t)(((uint64_t)(expected)) >> 32),    \
                                 (uint32_t)(((uint64_t)(actual)) >> 32));      \
        TEST_ASSERT_EQUAL_UINT32((uint32_t)((uint64_t)(expected)),             \
                                 (uint32_t)((uint64_t)(actual)));              \
    } while (0)
#endif

/* Enable floating-point assertions for meteorological calculations */
#define UNITY_INCLUDE_FLOAT
#define UNITY_INCLUDE_DOUBLE
#define UNITY_FLOAT_VERBOSE

/* Enable colorized terminal reporting */
#define UNITY_OUTPUT_COLOR

/* Standard output macro redirection */
#define UNITY_OUTPUT_CHAR(c)    putchar(c)
#define UNITY_OUTPUT_FLUSH()    fflush(stdout)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_CONFIG_H */
