/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#include "unity.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* Global state singleton */
struct UNITY_STORAGE_T Unity;

static const char UnityStrOk[]                = "OK";
static const char UnityStrPass[]              = "PASS";
static const char UnityStrFail[]              = "FAIL";
static const char UnityStrIgnore[]            = "IGNORE";
static const char UnityStrSpacer[]            = ". ";
static const char UnityStrExpected[]          = " Expected ";
static const char UnityStrWas[]               = " Was ";
static const char UnityStrNull[]              = "NULL";

#ifdef UNITY_OUTPUT_COLOR
static const char UnityStrColorGreen[]        = "\033[42;30m";
static const char UnityStrColorRed[]          = "\033[41;37m";
static const char UnityStrColorYellow[]       = "\033[43;30m";
static const char UnityStrColorReset[]        = "\033[00m";
#endif

/* -------------------------------------------------------------------------
 * Output Formatting Helpers
 * ------------------------------------------------------------------------- */
void UnityPrint(const char* string)
{
    const char* pch = string;
    if (pch != NULL)
    {
        while (*pch)
        {
            UNITY_OUTPUT_CHAR(*pch);
            pch++;
        }
    }
}

void UnityPrintLen(const char* string, const UNITY_UINT_TYPE length)
{
    const char* pch = string;
    UNITY_UINT_TYPE i = 0;
    if (pch != NULL)
    {
        while (*pch && (i < length))
        {
            UNITY_OUTPUT_CHAR(*pch);
            pch++;
            i++;
        }
    }
}

void UnityPrintNumberByStyle(const UNITY_INT_TYPE number, const int print_style)
{
    switch (print_style)
    {
        case UNITY_DISPLAY_STYLE_HEX8:
            UnityPrintNumberHex((UNITY_UINT_TYPE)number, 2);
            break;
        case UNITY_DISPLAY_STYLE_HEX16:
            UnityPrintNumberHex((UNITY_UINT_TYPE)number, 4);
            break;
        case UNITY_DISPLAY_STYLE_HEX32:
            UnityPrintNumberHex((UNITY_UINT_TYPE)number, 8);
            break;
        case UNITY_DISPLAY_STYLE_UINT:
        case UNITY_DISPLAY_STYLE_UINT8:
        case UNITY_DISPLAY_STYLE_UINT16:
        case UNITY_DISPLAY_STYLE_UINT32:
            UnityPrintNumberUnsigned((UNITY_UINT_TYPE)number);
            break;
        default:
            UnityPrintNumber(number);
            break;
    }
}

void UnityPrintNumber(const UNITY_INT_TYPE number)
{
    UNITY_INT_TYPE n = number;
    if (n < 0)
    {
        UNITY_OUTPUT_CHAR('-');
        n = -n;
    }
    UnityPrintNumberUnsigned((UNITY_UINT_TYPE)n);
}

void UnityPrintNumberUnsigned(const UNITY_UINT_TYPE number)
{
    UNITY_UINT_TYPE divisor = 1;
    UNITY_UINT_TYPE temp = number;

    while (temp >= 10)
    {
        divisor *= 10;
        temp /= 10;
    }

    temp = number;
    while (divisor > 0)
    {
        UNITY_OUTPUT_CHAR((char)('0' + (temp / divisor)));
        temp %= divisor;
        divisor /= 10;
    }
}

void UnityPrintNumberHex(const UNITY_UINT_TYPE number, const char nibbles)
{
    char nibble;
    char n = nibbles;

    UnityPrint("0x");
    while (n > 0)
    {
        n--;
        nibble = (char)((number >> (n * 4)) & 0x0F);
        if (nibble <= 9)
        {
            UNITY_OUTPUT_CHAR((char)('0' + nibble));
        }
        else
        {
            UNITY_OUTPUT_CHAR((char)('A' + (nibble - 10)));
        }
    }
}

void UnityPrintMask(const UNITY_UINT_TYPE mask, const UNITY_UINT_TYPE number)
{
    UNITY_UINT_TYPE current_bit = (UNITY_UINT_TYPE)1 << ((sizeof(UNITY_INT_TYPE) * 8) - 1);
    while (current_bit)
    {
        if (current_bit & mask)
        {
            UNITY_OUTPUT_CHAR((number & current_bit) ? '1' : '0');
        }
        else
        {
            UNITY_OUTPUT_CHAR('X');
        }
        current_bit >>= 1;
    }
}

#ifdef UNITY_INCLUDE_FLOAT
void UnityPrintFloat(const double number)
{
    char buffer[64];
    (void)snprintf(buffer, sizeof(buffer), "%.6f", number);
    UnityPrint(buffer);
}
#endif

/* -------------------------------------------------------------------------
 * Test Status & Results Output
 * ------------------------------------------------------------------------- */
void UnityTestResultsBegin(const char* file, const UNITY_UINT_TYPE line)
{
    UnityPrint(file);
    UNITY_OUTPUT_CHAR(':');
    UnityPrintNumberUnsigned(line);
    UNITY_OUTPUT_CHAR(':');
    UnityPrint(Unity.CurrentTestName);
    UNITY_OUTPUT_CHAR(':');
}

void UnityTestResultsFailBegin(const UNITY_UINT_TYPE line)
{
    UnityTestResultsBegin(Unity.TestFile, line);
    UnityPrint(UnityStrFail);
    UNITY_OUTPUT_CHAR(':');
}

void UnityConcludeTest(void)
{
    if (Unity.CurrentTestIgnored)
    {
        Unity.TestIgnores++;
    }
    else if (!Unity.CurrentTestFailed)
    {
        UnityTestResultsBegin(Unity.TestFile, Unity.CurrentTestLineNumber);
        UnityPrint(UnityStrPass);
        UNITY_PRINT_EOL();
    }
    else
    {
        Unity.TestFailures++;
    }
}

void UnityAddMsgIfSpecified(const char* msg)
{
    if (msg != NULL)
    {
        UnityPrint(UnityStrSpacer);
        UnityPrint(msg);
    }
}

void UnityFail(const char* msg, const UNITY_UINT_TYPE line)
{
    UnityTestResultsFailBegin(line);
    UnityAddMsgIfSpecified(msg);
    UNITY_PRINT_EOL();
    Unity.CurrentTestFailed = 1;
    longjmp(Unity.AbortFrame, 1);
}

void UnityIgnore(const char* msg, const UNITY_UINT_TYPE line)
{
    UnityTestResultsBegin(Unity.TestFile, line);
#ifdef UNITY_OUTPUT_COLOR
    UnityPrint(UnityStrColorYellow);
#endif
    UnityPrint(UnityStrIgnore);
#ifdef UNITY_OUTPUT_COLOR
    UnityPrint(UnityStrColorReset);
#endif
    UnityAddMsgIfSpecified(msg);
    UNITY_PRINT_EOL();
    Unity.CurrentTestIgnored = 1;
    longjmp(Unity.AbortFrame, 1);
}

/* -------------------------------------------------------------------------
 * Assertions Implementation
 * ------------------------------------------------------------------------- */
void UnityAssertEqualNumber(const UNITY_INT_TYPE expected,
                            const UNITY_INT_TYPE actual,
                            const char* msg,
                            const UNITY_UINT_TYPE line,
                            const int style)
{
    if (expected != actual)
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        UnityPrintNumberByStyle(expected, style);
        UnityPrint(UnityStrWas);
        UnityPrintNumberByStyle(actual, style);
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualIntArray(const void* expected,
                              const void* actual,
                              const UNITY_UINT_TYPE num_elements,
                              const char* msg,
                              const UNITY_UINT_TYPE line,
                              const int style)
{
    UNITY_UINT_TYPE elements = num_elements;
    const uint8_t* ptr_exp = (const uint8_t*)expected;
    const uint8_t* ptr_act = (const uint8_t*)actual;

    if (elements == 0)
    {
        return;
    }

    if ((expected == NULL) || (actual == NULL))
    {
        UnityFail("Null Pointer in Array Comparison", line);
    }

    for (UNITY_UINT_TYPE i = 0; i < elements; i++)
    {
        UNITY_INT_TYPE val_exp = 0;
        UNITY_INT_TYPE val_act = 0;

        switch (style)
        {
            case UNITY_DISPLAY_STYLE_INT8:
                val_exp = (int8_t)ptr_exp[i];
                val_act = (int8_t)ptr_act[i];
                break;
            case UNITY_DISPLAY_STYLE_HEX8:
            case UNITY_DISPLAY_STYLE_UINT8:
                val_exp = ptr_exp[i];
                val_act = ptr_act[i];
                break;
            case UNITY_DISPLAY_STYLE_INT16:
                val_exp = ((const int16_t*)expected)[i];
                val_act = ((const int16_t*)actual)[i];
                break;
            case UNITY_DISPLAY_STYLE_HEX16:
            case UNITY_DISPLAY_STYLE_UINT16:
                val_exp = ((const uint16_t*)expected)[i];
                val_act = ((const uint16_t*)actual)[i];
                break;
            case UNITY_DISPLAY_STYLE_INT32:
            case UNITY_DISPLAY_STYLE_INT:
                val_exp = ((const int32_t*)expected)[i];
                val_act = ((const int32_t*)actual)[i];
                break;
            default:
                val_exp = ((const uint32_t*)expected)[i];
                val_act = ((const uint32_t*)actual)[i];
                break;
        }

        if (val_exp != val_act)
        {
            UnityTestResultsFailBegin(line);
            UnityPrint(" Element ");
            UnityPrintNumberUnsigned(i);
            UnityPrint(UnityStrExpected);
            UnityPrintNumberByStyle(val_exp, style);
            UnityPrint(UnityStrWas);
            UnityPrintNumberByStyle(val_act, style);
            UnityAddMsgIfSpecified(msg);
            UNITY_PRINT_EOL();
            Unity.CurrentTestFailed = 1;
            longjmp(Unity.AbortFrame, 1);
        }
    }
}

void UnityAssertBits(const UNITY_INT_TYPE mask,
                     const UNITY_INT_TYPE expected,
                     const UNITY_INT_TYPE actual,
                     const char* msg,
                     const UNITY_UINT_TYPE line)
{
    if ((expected & mask) != (actual & mask))
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        UnityPrintMask((UNITY_UINT_TYPE)mask, (UNITY_UINT_TYPE)expected);
        UnityPrint(UnityStrWas);
        UnityPrintMask((UNITY_UINT_TYPE)mask, (UNITY_UINT_TYPE)actual);
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_UINT_TYPE line)
{
    if ((expected == NULL) && (actual == NULL))
    {
        return;
    }
    if ((expected == NULL) || (actual == NULL) || (strcmp(expected, actual) != 0))
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        UnityPrint((expected == NULL) ? UnityStrNull : expected);
        UnityPrint(UnityStrWas);
        UnityPrint((actual == NULL) ? UnityStrNull : actual);
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualStringLen(const char* expected,
                               const char* actual,
                               const UNITY_UINT_TYPE length,
                               const char* msg,
                               const UNITY_UINT_TYPE line)
{
    if ((expected == NULL) && (actual == NULL))
    {
        return;
    }
    if ((expected == NULL) || (actual == NULL) || (strncmp(expected, actual, length) != 0))
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        if (expected == NULL) { UnityPrint(UnityStrNull); } else { UnityPrintLen(expected, length); }
        UnityPrint(UnityStrWas);
        if (actual == NULL) { UnityPrint(UnityStrNull); } else { UnityPrintLen(actual, length); }
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualMemory(const void* expected,
                            const void* actual,
                            const UNITY_UINT_TYPE length,
                            const UNITY_UINT_TYPE num_elements,
                            const char* msg,
                            const UNITY_UINT_TYPE line)
{
    const uint8_t* ptr_exp = (const uint8_t*)expected;
    const uint8_t* ptr_act = (const uint8_t*)actual;
    UNITY_UINT_TYPE total_bytes = length * num_elements;

    if (total_bytes == 0)
    {
        return;
    }

    if ((expected == NULL) || (actual == NULL))
    {
        UnityFail("Null Pointer in Memory Comparison", line);
    }

    for (UNITY_UINT_TYPE i = 0; i < total_bytes; i++)
    {
        if (ptr_exp[i] != ptr_act[i])
        {
            UnityTestResultsFailBegin(line);
            UnityPrint(" Memory Mismatch at Byte ");
            UnityPrintNumberUnsigned(i);
            UnityPrint(UnityStrExpected);
            UnityPrintNumberHex(ptr_exp[i], 2);
            UnityPrint(UnityStrWas);
            UnityPrintNumberHex(ptr_act[i], 2);
            UnityAddMsgIfSpecified(msg);
            UNITY_PRINT_EOL();
            Unity.CurrentTestFailed = 1;
            longjmp(Unity.AbortFrame, 1);
        }
    }
}

void UnityAssertNumbersWithin(const UNITY_UINT_TYPE delta,
                              const UNITY_INT_TYPE expected,
                              const UNITY_INT_TYPE actual,
                              const char* msg,
                              const UNITY_UINT_TYPE line,
                              const int style)
{
    UNITY_INT_TYPE diff = actual - expected;
    if (diff < 0)
    {
        diff = -diff;
    }

    if ((UNITY_UINT_TYPE)diff > delta)
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        UnityPrintNumberByStyle(expected, style);
        UnityPrint(UnityStrWas);
        UnityPrintNumberByStyle(actual, style);
        UnityPrint(" (Diff: ");
        UnityPrintNumberByStyle(diff, style);
        UnityPrint(" > Delta: ");
        UnityPrintNumberByStyle((UNITY_INT_TYPE)delta, style);
        UnityPrint(")");
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

#ifdef UNITY_INCLUDE_FLOAT
void UnityAssertFloatsWithin(const UNITY_FLOAT_TYPE delta,
                             const UNITY_FLOAT_TYPE expected,
                             const UNITY_FLOAT_TYPE actual,
                             const char* msg,
                             const UNITY_UINT_TYPE line)
{
    UNITY_FLOAT_TYPE diff = actual - expected;
    if (diff < 0.0f)
    {
        diff = -diff;
    }

    if ((isnan(expected) && !isnan(actual)) ||
        (!isnan(expected) && isnan(actual)) ||
        (diff > delta))
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        UnityPrintFloat((double)expected);
        UnityPrint(UnityStrWas);
        UnityPrintFloat((double)actual);
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualFloatArray(const UNITY_FLOAT_TYPE* expected,
                                const UNITY_FLOAT_TYPE* actual,
                                const UNITY_UINT_TYPE num_elements,
                                const char* msg,
                                const UNITY_UINT_TYPE line)
{
    if (num_elements == 0)
    {
        return;
    }
    if ((expected == NULL) || (actual == NULL))
    {
        UnityFail("Null Pointer in Float Array Comparison", line);
    }

    for (UNITY_UINT_TYPE i = 0; i < num_elements; i++)
    {
        UNITY_FLOAT_TYPE diff = actual[i] - expected[i];
        if (diff < 0.0f)
        {
            diff = -diff;
        }
        if (diff > UNITY_FLOAT_PRECISION)
        {
            UnityTestResultsFailBegin(line);
            UnityPrint(" Float Array Element ");
            UnityPrintNumberUnsigned(i);
            UnityPrint(UnityStrExpected);
            UnityPrintFloat((double)expected[i]);
            UnityPrint(UnityStrWas);
            UnityPrintFloat((double)actual[i]);
            UnityAddMsgIfSpecified(msg);
            UNITY_PRINT_EOL();
            Unity.CurrentTestFailed = 1;
            longjmp(Unity.AbortFrame, 1);
        }
    }
}

void UnityAssertFloatSpecial(const UNITY_FLOAT_TYPE actual,
                             const char* msg,
                             const UNITY_UINT_TYPE line,
                             const int style)
{
    int pass = 0;
    switch (style)
    {
        case UNITY_FLOAT_IS_INF:
            pass = isinf(actual) && (actual > 0.0f);
            break;
        case UNITY_FLOAT_IS_NEG_INF:
            pass = isinf(actual) && (actual < 0.0f);
            break;
        case UNITY_FLOAT_IS_NAN:
            pass = isnan(actual);
            break;
        case UNITY_FLOAT_IS_DET:
            pass = !isnan(actual) && !isinf(actual);
            break;
        default:
            break;
    }

    if (!pass)
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(" Float Special Condition Failed");
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}
#endif

#ifdef UNITY_INCLUDE_DOUBLE
void UnityAssertDoublesWithin(const UNITY_DOUBLE_TYPE delta,
                              const UNITY_DOUBLE_TYPE expected,
                              const UNITY_DOUBLE_TYPE actual,
                              const char* msg,
                              const UNITY_UINT_TYPE line)
{
    UNITY_DOUBLE_TYPE diff = actual - expected;
    if (diff < 0.0)
    {
        diff = -diff;
    }

    if ((isnan(expected) && !isnan(actual)) ||
        (!isnan(expected) && isnan(actual)) ||
        (diff > delta))
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(UnityStrExpected);
        UnityPrintFloat(expected);
        UnityPrint(UnityStrWas);
        UnityPrintFloat(actual);
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}

void UnityAssertEqualDoubleArray(const UNITY_DOUBLE_TYPE* expected,
                                 const UNITY_DOUBLE_TYPE* actual,
                                 const UNITY_UINT_TYPE num_elements,
                                 const char* msg,
                                 const UNITY_UINT_TYPE line)
{
    if (num_elements == 0)
    {
        return;
    }
    if ((expected == NULL) || (actual == NULL))
    {
        UnityFail("Null Pointer in Double Array Comparison", line);
    }

    for (UNITY_UINT_TYPE i = 0; i < num_elements; i++)
    {
        UNITY_DOUBLE_TYPE diff = actual[i] - expected[i];
        if (diff < 0.0)
        {
            diff = -diff;
        }
        if (diff > UNITY_DOUBLE_PRECISION)
        {
            UnityTestResultsFailBegin(line);
            UnityPrint(" Double Array Element ");
            UnityPrintNumberUnsigned(i);
            UnityPrint(UnityStrExpected);
            UnityPrintFloat(expected[i]);
            UnityPrint(UnityStrWas);
            UnityPrintFloat(actual[i]);
            UnityAddMsgIfSpecified(msg);
            UNITY_PRINT_EOL();
            Unity.CurrentTestFailed = 1;
            longjmp(Unity.AbortFrame, 1);
        }
    }
}

void UnityAssertDoubleSpecial(const UNITY_DOUBLE_TYPE actual,
                              const char* msg,
                              const UNITY_UINT_TYPE line,
                              const int style)
{
    int pass = 0;
    switch (style)
    {
        case UNITY_FLOAT_IS_INF:
            pass = isinf(actual) && (actual > 0.0);
            break;
        case UNITY_FLOAT_IS_NEG_INF:
            pass = isinf(actual) && (actual < 0.0);
            break;
        case UNITY_FLOAT_IS_NAN:
            pass = isnan(actual);
            break;
        case UNITY_FLOAT_IS_DET:
            pass = !isnan(actual) && !isinf(actual);
            break;
        default:
            break;
    }

    if (!pass)
    {
        UnityTestResultsFailBegin(line);
        UnityPrint(" Double Special Condition Failed");
        UnityAddMsgIfSpecified(msg);
        UNITY_PRINT_EOL();
        Unity.CurrentTestFailed = 1;
        longjmp(Unity.AbortFrame, 1);
    }
}
#endif

/* -------------------------------------------------------------------------
 * Test Suite Lifecycle Management
 * ------------------------------------------------------------------------- */
void UnityBegin(const char* filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestLineNumber = 0;
    Unity.CurrentTestIgnored = 0;
    Unity.CurrentTestFailed = 0;
    UNITY_OUTPUT_START();
}

int UnityEnd(void)
{
    UNITY_PRINT_EOL();
    UnityPrint("-----------------------");
    UNITY_PRINT_EOL();
    UnityPrintNumberUnsigned(Unity.NumberOfTests);
    UnityPrint(" Tests ");
    UnityPrintNumberUnsigned(Unity.TestFailures);
    UnityPrint(" Failures ");
    UnityPrintNumberUnsigned(Unity.TestIgnores);
    UnityPrint(" Ignored");
    UNITY_PRINT_EOL();

    if (Unity.TestFailures == 0U)
    {
#ifdef UNITY_OUTPUT_COLOR
        UnityPrint(UnityStrColorGreen);
#endif
        UnityPrint(UnityStrOk);
#ifdef UNITY_OUTPUT_COLOR
        UnityPrint(UnityStrColorReset);
#endif
    }
    else
    {
#ifdef UNITY_OUTPUT_COLOR
        UnityPrint(UnityStrColorRed);
#endif
        UnityPrint(UnityStrFail);
#ifdef UNITY_OUTPUT_COLOR
        UnityPrint(UnityStrColorReset);
#endif
    }
    UNITY_PRINT_EOL();
    UNITY_OUTPUT_FLUSH();
    UNITY_OUTPUT_COMPLETE();

    return (int)Unity.TestFailures;
}

void UnityDefaultTestRun(void (*func)(void), const char* name, int line)
{
    Unity.CurrentTestName = name;
    Unity.CurrentTestLineNumber = (UNITY_UINT_TYPE)line;
    Unity.CurrentTestIgnored = 0;
    Unity.CurrentTestFailed = 0;
    Unity.NumberOfTests++;

    if (setjmp(Unity.AbortFrame) == 0)
    {
        setUp();
        func();
    }
    if (setjmp(Unity.AbortFrame) == 0)
    {
        tearDown();
    }
    UnityConcludeTest();
}
