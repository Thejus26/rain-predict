/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#ifdef UNITY_INCLUDE_CONFIG_H
#include "unity_config.h"
#endif

#include <setjmp.h>
#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>

/* Unity Integer Width Configuration */
#ifndef UNITY_INT_WIDTH
#define UNITY_INT_WIDTH 32
#endif

#ifndef UNITY_LONG_WIDTH
#define UNITY_LONG_WIDTH 32
#endif

#ifndef UNITY_POINTER_WIDTH
#define UNITY_POINTER_WIDTH 32
#endif

/* Basic Types */
typedef uint32_t _UU32;
typedef int32_t  _US32;
typedef uint16_t _UU16;
typedef int16_t  _US16;
typedef uint8_t  _UU8;
typedef int8_t   _US8;

#ifndef UNITY_INT_TYPE
#define UNITY_INT_TYPE int32_t
#endif

#ifndef UNITY_UINT_TYPE
#define UNITY_UINT_TYPE uint32_t
#endif

/* Float Support */
#ifdef UNITY_INCLUDE_FLOAT
#define UNITY_FLOAT_TYPE float
#ifndef UNITY_FLOAT_PRECISION
#define UNITY_FLOAT_PRECISION 0.00001f
#endif
#endif

#ifdef UNITY_INCLUDE_DOUBLE
#define UNITY_DOUBLE_TYPE double
#ifndef UNITY_DOUBLE_PRECISION
#define UNITY_DOUBLE_PRECISION 0.000000001
#endif
#endif

/* Output handling */
#ifndef UNITY_OUTPUT_CHAR
#include <stdio.h>
#define UNITY_OUTPUT_CHAR(a) putchar(a)
#endif

#ifndef UNITY_OUTPUT_FLUSH
#include <stdio.h>
#define UNITY_OUTPUT_FLUSH() fflush(stdout)
#endif

#ifndef UNITY_OUTPUT_START
#define UNITY_OUTPUT_START()
#endif

#ifndef UNITY_OUTPUT_COMPLETE
#define UNITY_OUTPUT_COMPLETE()
#endif

#ifndef UNITY_PRINT_EOL
#define UNITY_PRINT_EOL() { UNITY_OUTPUT_CHAR('\r'); UNITY_OUTPUT_CHAR('\n'); }
#endif

/* Unity Test Structure */
struct UNITY_STORAGE_T
{
    const char* TestFile;
    const char* CurrentTestName;
    UNITY_UINT_TYPE NumberOfTests;
    UNITY_UINT_TYPE TestFailures;
    UNITY_UINT_TYPE TestIgnores;
    UNITY_UINT_TYPE CurrentTestLineNumber;
    UNITY_UINT_TYPE CurrentTestIgnored;
    UNITY_UINT_TYPE CurrentTestFailed;
    jmp_buf AbortFrame;
};

extern struct UNITY_STORAGE_T Unity;

/* Test Lifecycle Functions */
void UnityBegin(const char* filename);
int  UnityEnd(void);
void UnityDefaultTestRun(void (*func)(void), const char* name, int line);

/* Assertion Detail Printers */
void UnityPrint(const char* string);
void UnityPrintLen(const char* string, const UNITY_UINT_TYPE length);
void UnityPrintMask(const UNITY_UINT_TYPE mask, const UNITY_UINT_TYPE number);
void UnityPrintNumberByStyle(const UNITY_INT_TYPE number, const int print_style);
void UnityPrintNumber(const UNITY_INT_TYPE number);
void UnityPrintNumberUnsigned(const UNITY_UINT_TYPE number);
void UnityPrintNumberHex(const UNITY_UINT_TYPE number, const char nibbles);

#ifdef UNITY_INCLUDE_FLOAT
void UnityPrintFloat(const double number);
#endif

/* Failure & Ignore Triggers */
void UnityTestResultsBegin(const char* file, const UNITY_UINT_TYPE line);
void UnityTestResultsFailBegin(const UNITY_UINT_TYPE line);
void UnityConcludeTest(void);
void UnityAddMsgIfSpecified(const char* msg);
void UnityFail(const char* msg, const UNITY_UINT_TYPE line);
void UnityIgnore(const char* msg, const UNITY_UINT_TYPE line);

/* Display Styles */
#define UNITY_DISPLAY_STYLE_INT         0
#define UNITY_DISPLAY_STYLE_INT8        1
#define UNITY_DISPLAY_STYLE_INT16       2
#define UNITY_DISPLAY_STYLE_INT32       3
#define UNITY_DISPLAY_STYLE_UINT        4
#define UNITY_DISPLAY_STYLE_UINT8       5
#define UNITY_DISPLAY_STYLE_UINT16      6
#define UNITY_DISPLAY_STYLE_UINT32      7
#define UNITY_DISPLAY_STYLE_HEX8        8
#define UNITY_DISPLAY_STYLE_HEX16       9
#define UNITY_DISPLAY_STYLE_HEX32       10
#define UNITY_DISPLAY_STYLE_UNKNOWN     11

/* Comparison & Assertion Functions */
void UnityAssertEqualNumber(const UNITY_INT_TYPE expected,
                            const UNITY_INT_TYPE actual,
                            const char* msg,
                            const UNITY_UINT_TYPE line,
                            const int style);

void UnityAssertEqualIntArray(const void* expected,
                              const void* actual,
                              const UNITY_UINT_TYPE num_elements,
                              const char* msg,
                              const UNITY_UINT_TYPE line,
                              const int style);

void UnityAssertBits(const UNITY_INT_TYPE mask,
                     const UNITY_INT_TYPE expected,
                     const UNITY_INT_TYPE actual,
                     const char* msg,
                     const UNITY_UINT_TYPE line);

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_UINT_TYPE line);

void UnityAssertEqualStringLen(const char* expected,
                               const char* actual,
                               const UNITY_UINT_TYPE length,
                               const char* msg,
                               const UNITY_UINT_TYPE line);

void UnityAssertEqualMemory(const void* expected,
                            const void* actual,
                            const UNITY_UINT_TYPE length,
                            const UNITY_UINT_TYPE num_elements,
                            const char* msg,
                            const UNITY_UINT_TYPE line);

void UnityAssertNumbersWithin(const UNITY_UINT_TYPE delta,
                              const UNITY_INT_TYPE expected,
                              const UNITY_INT_TYPE actual,
                              const char* msg,
                              const UNITY_UINT_TYPE line,
                              const int style);

#ifdef UNITY_INCLUDE_FLOAT
void UnityAssertFloatsWithin(const UNITY_FLOAT_TYPE delta,
                             const UNITY_FLOAT_TYPE expected,
                             const UNITY_FLOAT_TYPE actual,
                             const char* msg,
                             const UNITY_UINT_TYPE line);

void UnityAssertEqualFloatArray(const UNITY_FLOAT_TYPE* expected,
                                const UNITY_FLOAT_TYPE* actual,
                                const UNITY_UINT_TYPE num_elements,
                                const char* msg,
                                const UNITY_UINT_TYPE line);

void UnityAssertFloatSpecial(const UNITY_FLOAT_TYPE actual,
                             const char* msg,
                             const UNITY_UINT_TYPE line,
                             const int style);
#endif

#ifdef UNITY_INCLUDE_DOUBLE
void UnityAssertDoublesWithin(const UNITY_DOUBLE_TYPE delta,
                              const UNITY_DOUBLE_TYPE expected,
                              const UNITY_DOUBLE_TYPE actual,
                              const char* msg,
                              const UNITY_UINT_TYPE line);

void UnityAssertEqualDoubleArray(const UNITY_DOUBLE_TYPE* expected,
                                 const UNITY_DOUBLE_TYPE* actual,
                                 const UNITY_UINT_TYPE num_elements,
                                 const char* msg,
                                 const UNITY_UINT_TYPE line);

void UnityAssertDoubleSpecial(const UNITY_DOUBLE_TYPE actual,
                              const char* msg,
                              const UNITY_UINT_TYPE line,
                              const int style);
#endif

#define UNITY_FLOAT_IS_NOT_INF      0
#define UNITY_FLOAT_IS_INF          1
#define UNITY_FLOAT_IS_NOT_NEG_INF  2
#define UNITY_FLOAT_IS_NEG_INF      3
#define UNITY_FLOAT_IS_NOT_NAN      4
#define UNITY_FLOAT_IS_NAN          5
#define UNITY_FLOAT_IS_NOT_DET      6
#define UNITY_FLOAT_IS_DET          7

#endif /* UNITY_INTERNALS_H */
