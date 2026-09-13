# Error Report: ERR-022 - Non-Existent TEST_ASSERT_UINT16_WITHIN Macro in test_bsp_adc.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-022` |
| **Date & Time** | `2026-09-13 17:26:21 +0530` |
| **Commit SHA** | [`108811a`](https://github.com/Thejus26/rain-predict/commit/108811ab36ef4a6e8b647e4103775a9cb7c3673c) |
| **Sprint / Task** | Sprint 3 (S3-T4.4 Battery ADC Telemetry & Unity Tests) |
| **Severity** | High (Build Breakage / Host Compilation Failure) |
| **Impacted Files** | [`tests/unit/test_bsp_adc.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bsp_adc.c) |

---

## 1. Description & Symptoms

When building host unit tests with GCC (`-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Werror`), compilation of [`tests/unit/test_bsp_adc.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bsp_adc.c) failed with an implicit function declaration error:

```text
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bsp_adc.c: In function ‘test_bsp_adc_vbat_mv_accuracy’:
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bsp_adc.c:140:5: error: implicit declaration of function ‘TEST_ASSERT_UINT16_WITHIN’; did you mean ‘TEST_ASSERT_FLOAT_WITHIN’? [-Werror=implicit-function-declaration]
  140 |     TEST_ASSERT_UINT16_WITHIN(15U, 3000U, vbat_mv);
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_FLOAT_WITHIN
cc1: all warnings being treated as errors
```

---

## 2. Root Cause Analysis

[`tests/unit/test_bsp_adc.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bsp_adc.c) attempted to assert that measured battery voltages were within $\pm 15\text{ mV}$ of target voltages using `TEST_ASSERT_UINT16_WITHIN()`. 

However, the ThrowTheSwitch Unity framework header ([`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h)) only defines `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)` and `TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)`. It does not provide `TEST_ASSERT_UINT16_WITHIN()`. Under `-Werror=implicit-function-declaration`, the compiler failed.

---

## 3. Resolution & Code Changes

Defined the local test helper macro `TEST_ASSERT_UINT16_WITHIN` in [`tests/unit/test_bsp_adc.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bsp_adc.c):

```diff
--- a/tests/unit/test_bsp_adc.c
+++ b/tests/unit/test_bsp_adc.c
@@ -14,6 +14,11 @@
 #include "bsp_power_rails.h"
 #include "board_config.h"
 
+/** @brief Local helper macro for integer tolerance assertion within +/- delta */
+#define TEST_ASSERT_UINT16_WITHIN(delta, expected, actual) \
+    TEST_ASSERT_TRUE(((uint32_t)(actual) >= ((uint32_t)(expected) - (uint32_t)(delta))) && \
+                     ((uint32_t)(actual) <= ((uint32_t)(expected) + (uint32_t)(delta))))
+
 void setUp(void) {
```

---

## 4. Verification & Prevention Guidelines

1. **Unity Assertion Header Reference**: Always verify that Unity assertion macros exist in [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h).
2. **Explicit Integer Bounds**: Use `TEST_ASSERT_TRUE(val >= (expected - delta) && val <= (expected + delta))` for integer delta tolerance verifications.
