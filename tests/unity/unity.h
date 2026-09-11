/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "unity_internals.h"

/* -------------------------------------------------------------------------
 * Test Runner Management
 * ------------------------------------------------------------------------- */
#define UNITY_BEGIN()               UnityBegin(__FILE__)
#define UNITY_END()                 UnityEnd()

#define RUN_TEST(func)              UnityDefaultTestRun(func, #func, __LINE__)

/* setUp and tearDown hook prototypes */
void setUp(void);
void tearDown(void);

/* -------------------------------------------------------------------------
 * Basic Boolean Assertions
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT(condition)                                                \
    do {                                                                      \
        if (!(condition)) {                                                   \
            UnityAssertEqualNumber(1, 0, ( #condition ), __LINE__,            \
                                   UNITY_DISPLAY_STYLE_INT);                  \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_TRUE(condition)                                           \
    do {                                                                      \
        if (!(condition)) {                                                   \
            UnityAssertEqualNumber(1, 0, " Expected TRUE Was FALSE",          \
                                   __LINE__, UNITY_DISPLAY_STYLE_INT);        \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_FALSE(condition)                                          \
    do {                                                                      \
        if (condition) {                                                      \
            UnityAssertEqualNumber(0, 1, " Expected FALSE Was TRUE",         \
                                   __LINE__, UNITY_DISPLAY_STYLE_INT);        \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_UNLESS(condition)                                         \
    TEST_ASSERT_FALSE(condition)

#define TEST_FAIL()                                                           \
    UnityFail(NULL, __LINE__)

#define TEST_FAIL_MESSAGE(message)                                            \
    UnityFail((message), __LINE__)

#define TEST_IGNORE()                                                         \
    UnityIgnore(NULL, __LINE__)

#define TEST_IGNORE_MESSAGE(message)                                          \
    UnityIgnore((message), __LINE__)

/* -------------------------------------------------------------------------
 * Integer Assertions (Signed & Unsigned)
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT_EQUAL_INT(expected, actual)                               \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(expected),                        \
                           (UNITY_INT_TYPE)(actual),                          \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_EQUAL_INT8(expected, actual)                              \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(int8_t)(expected),                \
                           (UNITY_INT_TYPE)(int8_t)(actual),                  \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_INT8)

#define TEST_ASSERT_EQUAL_INT16(expected, actual)                             \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(int16_t)(expected),               \
                           (UNITY_INT_TYPE)(int16_t)(actual),                 \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_INT16)

#define TEST_ASSERT_EQUAL_INT32(expected, actual)                             \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(int32_t)(expected),               \
                           (UNITY_INT_TYPE)(int32_t)(actual),                 \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_INT32)

#define TEST_ASSERT_EQUAL_UINT(expected, actual)                              \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(expected),                        \
                           (UNITY_INT_TYPE)(actual),                          \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)

#define TEST_ASSERT_EQUAL_UINT8(expected, actual)                             \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uint8_t)(expected),               \
                           (UNITY_INT_TYPE)(uint8_t)(actual),                 \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT8)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual)                            \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uint16_t)(expected),              \
                           (UNITY_INT_TYPE)(uint16_t)(actual),                \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT16)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                            \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uint32_t)(expected),              \
                           (UNITY_INT_TYPE)(uint32_t)(actual),                \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT32)

#define TEST_ASSERT_EQUAL(expected, actual)                                   \
    TEST_ASSERT_EQUAL_INT((expected), (actual))

/* -------------------------------------------------------------------------
 * Hexadecimal Assertions
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT_EQUAL_HEX(expected, actual)                               \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(expected),                        \
                           (UNITY_INT_TYPE)(actual),                          \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)

#define TEST_ASSERT_EQUAL_HEX8(expected, actual)                              \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uint8_t)(expected),               \
                           (UNITY_INT_TYPE)(uint8_t)(actual),                 \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX8)

#define TEST_ASSERT_EQUAL_HEX16(expected, actual)                             \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uint16_t)(expected),              \
                           (UNITY_INT_TYPE)(uint16_t)(actual),                \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX16)

#define TEST_ASSERT_EQUAL_HEX32(expected, actual)                             \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uint32_t)(expected),              \
                           (UNITY_INT_TYPE)(uint32_t)(actual),                \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)

/* -------------------------------------------------------------------------
 * Bitwise Assertions
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT_BITS(mask, expected, actual)                              \
    UnityAssertBits((UNITY_INT_TYPE)(mask),                                   \
                    (UNITY_INT_TYPE)(expected),                               \
                    (UNITY_INT_TYPE)(actual),                                 \
                    NULL, __LINE__)

#define TEST_ASSERT_BITS_HIGH(mask, actual)                                   \
    TEST_ASSERT_BITS((mask), (mask), (actual))

#define TEST_ASSERT_BITS_LOW(mask, actual)                                    \
    TEST_ASSERT_BITS((mask), 0, (actual))

#define TEST_ASSERT_BIT_HIGH(bit, actual)                                     \
    TEST_ASSERT_BITS(((UNITY_INT_TYPE)1 << (bit)),                            \
                     ((UNITY_INT_TYPE)1 << (bit)), (actual))

#define TEST_ASSERT_BIT_LOW(bit, actual)                                      \
    TEST_ASSERT_BITS(((UNITY_INT_TYPE)1 << (bit)), 0, (actual))

/* -------------------------------------------------------------------------
 * Pointer & Memory Assertions
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT_NULL(pointer)                                             \
    UnityAssertEqualNumber(0, (UNITY_INT_TYPE)(uintptr_t)(pointer),           \
                           " Expected NULL", __LINE__,                        \
                           UNITY_DISPLAY_STYLE_HEX32)

#define TEST_ASSERT_NOT_NULL(pointer)                                         \
    do {                                                                      \
        if ((pointer) == NULL) {                                              \
            UnityAssertEqualNumber(1, 0, " Expected Non-NULL",                \
                                   __LINE__, UNITY_DISPLAY_STYLE_INT);        \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_EQUAL_PTR(expected, actual)                               \
    UnityAssertEqualNumber((UNITY_INT_TYPE)(uintptr_t)(expected),             \
                           (UNITY_INT_TYPE)(uintptr_t)(actual),               \
                           NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)                       \
    UnityAssertEqualMemory((const void*)(expected),                           \
                           (const void*)(actual),                             \
                           (UNITY_UINT_TYPE)(len), 1,                         \
                           NULL, __LINE__)

/* -------------------------------------------------------------------------
 * String Assertions
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT_EQUAL_STRING(expected, actual)                            \
    UnityAssertEqualString((const char*)(expected),                           \
                           (const char*)(actual),                             \
                           NULL, __LINE__)

#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len)                   \
    UnityAssertEqualStringLen((const char*)(expected),                        \
                              (const char*)(actual),                          \
                              (UNITY_UINT_TYPE)(len),                         \
                              NULL, __LINE__)

/* -------------------------------------------------------------------------
 * Floating-Point Assertions
 * ------------------------------------------------------------------------- */
#ifdef UNITY_INCLUDE_FLOAT
#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)                     \
    UnityAssertFloatsWithin((UNITY_FLOAT_TYPE)(delta),                        \
                            (UNITY_FLOAT_TYPE)(expected),                     \
                            (UNITY_FLOAT_TYPE)(actual),                       \
                            NULL, __LINE__)

#define TEST_ASSERT_EQUAL_FLOAT(expected, actual)                             \
    TEST_ASSERT_FLOAT_WITHIN(UNITY_FLOAT_PRECISION, (expected), (actual))

#define TEST_ASSERT_FLOAT_IS_INF(actual)                                      \
    UnityAssertFloatSpecial((UNITY_FLOAT_TYPE)(actual), NULL, __LINE__,       \
                            UNITY_FLOAT_IS_INF)

#define TEST_ASSERT_FLOAT_IS_NEG_INF(actual)                                  \
    UnityAssertFloatSpecial((UNITY_FLOAT_TYPE)(actual), NULL, __LINE__,       \
                            UNITY_FLOAT_IS_NEG_INF)

#define TEST_ASSERT_FLOAT_IS_NAN(actual)                                      \
    UnityAssertFloatSpecial((UNITY_FLOAT_TYPE)(actual), NULL, __LINE__,       \
                            UNITY_FLOAT_IS_NAN)

#define TEST_ASSERT_FLOAT_IS_DETERMINATE(actual)                              \
    UnityAssertFloatSpecial((UNITY_FLOAT_TYPE)(actual), NULL, __LINE__,       \
                            UNITY_FLOAT_IS_DET)
#endif

#ifdef UNITY_INCLUDE_DOUBLE
#define TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)                    \
    UnityAssertDoublesWithin((UNITY_DOUBLE_TYPE)(delta),                      \
                             (UNITY_DOUBLE_TYPE)(expected),                   \
                             (UNITY_DOUBLE_TYPE)(actual),                     \
                             NULL, __LINE__)

#define TEST_ASSERT_EQUAL_DOUBLE(expected, actual)                            \
    TEST_ASSERT_DOUBLE_WITHIN(UNITY_DOUBLE_PRECISION, (expected), (actual))
#endif

/* -------------------------------------------------------------------------
 * Array Assertions
 * ------------------------------------------------------------------------- */
#define TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)           \
    UnityAssertEqualIntArray((const void*)(expected),                         \
                             (const void*)(actual),                           \
                             (UNITY_UINT_TYPE)(num_elements),                 \
                             NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements)         \
    UnityAssertEqualIntArray((const void*)(expected),                         \
                             (const void*)(actual),                           \
                             (UNITY_UINT_TYPE)(num_elements),                 \
                             NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT8)

#define TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, num_elements)          \
    UnityAssertEqualIntArray((const void*)(expected),                         \
                             (const void*)(actual),                           \
                             (UNITY_UINT_TYPE)(num_elements),                 \
                             NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX8)

#ifdef UNITY_INCLUDE_FLOAT
#define TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements)         \
    UnityAssertEqualFloatArray((const UNITY_FLOAT_TYPE*)(expected),           \
                               (const UNITY_FLOAT_TYPE*)(actual),             \
                               (UNITY_UINT_TYPE)(num_elements),               \
                               NULL, __LINE__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UNITY_FRAMEWORK_H */
