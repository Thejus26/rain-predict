# Error Report: ERR-024 - Non-Existent Integer Tolerance Assertion Macros in test_bme280.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-024` |
| **Date & Time** | `2026-09-14 08:24:23 +0530` |
| **Commit SHA** | [`c8c8279`](https://github.com/Thejus26/rain-predict/commit/c8c8279c7d5e5b0b5e7f261ce0d551e1cdc9459c) |
| **Sprint / Task** | Sprint 4 (S4-T1.3 BME280 FPU Compensation Calculations & Fixed-Point Conversion) |
| **Severity** | High (Build Breakage / Host CI Compilation Failure) |
| **Impacted Files** | [`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c) |

---

## 1. Description & Symptoms

During CI / host build compilation of unit test binaries with GCC (`-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror`), compilation of [`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c) failed with multiple implicit function declaration errors:

```text
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c: In function ‘test_bme280_compensation_fixed_point_scaling’:
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c:641:5: error: implicit declaration of function ‘TEST_ASSERT_INT16_WITHIN’; did you mean ‘TEST_ASSERT_FLOAT_WITHIN’? [-Werror=implicit-function-declaration]
  641 |     TEST_ASSERT_INT16_WITHIN(5, 2508, fixed_data.temp_centi_c);
      |     ^~~~~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_FLOAT_WITHIN
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c:643:5: error: implicit declaration of function ‘TEST_ASSERT_UINT32_WITHIN’; did you mean ‘TEST_ASSERT_FLOAT_WITHIN’? [-Werror=implicit-function-declaration]
  643 |     TEST_ASSERT_UINT32_WITHIN(10, 100653U, fixed_data.press_pascals);
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_FLOAT_WITHIN
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c:645:5: error: implicit declaration of function ‘TEST_ASSERT_UINT16_WITHIN’; did you mean ‘TEST_ASSERT_FLOAT_WITHIN’? [-Werror=implicit-function-declaration]
  645 |     TEST_ASSERT_UINT16_WITHIN(10, 5432U, fixed_data.hum_centi_percent);
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_FLOAT_WITHIN
cc1: all warnings being treated as errors
ninja: build stopped: subcommand failed.
```

---

## 2. Root Cause Analysis

[`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c) tested the fixed-point scaling conversions in `test_bme280_compensation_fixed_point_scaling()` by calling `TEST_ASSERT_INT16_WITHIN`, `TEST_ASSERT_UINT32_WITHIN`, and `TEST_ASSERT_UINT16_WITHIN`.

However, the ThrowTheSwitch Unity test framework ([`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h)) only includes floating-point tolerance macros (`TEST_ASSERT_FLOAT_WITHIN`, `TEST_ASSERT_DOUBLE_WITHIN`), or exact integer assertions (`TEST_ASSERT_EQUAL_INT16`, `TEST_ASSERT_EQUAL_UINT32`, etc.). It does not define delta-range tolerance macros for integer types. Under strict C99 `-Werror=implicit-function-declaration`, the undefined macro calls caused fatal compilation errors.

---

## 3. Resolution & Code Changes

Defined local test helper macros `TEST_ASSERT_INT16_WITHIN`, `TEST_ASSERT_UINT32_WITHIN`, and `TEST_ASSERT_UINT16_WITHIN` in [`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c) with explicit bounds evaluation and 32-bit cast guards.

```diff
--- a/tests/unit/test_bme280.c
+++ b/tests/unit/test_bme280.c
@@ -18,6 +18,19 @@
 #include "i2c_bus.h"
 #include "bme280_driver.h"
 
+/** @brief Local helper macros for integer tolerance assertions within +/- delta */
+#define TEST_ASSERT_INT16_WITHIN(delta, expected, actual) \
+    TEST_ASSERT_TRUE(((int32_t)(actual) >= ((int32_t)(expected) - (int32_t)(delta))) && \
+                     ((int32_t)(actual) <= ((int32_t)(expected) + (int32_t)(delta))))
+
+#define TEST_ASSERT_UINT32_WITHIN(delta, expected, actual) \
+    TEST_ASSERT_TRUE(((uint32_t)(actual) >= ((uint32_t)(expected) - (uint32_t)(delta))) && \
+                     ((uint32_t)(actual) <= ((uint32_t)(expected) + (uint32_t)(delta))))
+
+#define TEST_ASSERT_UINT16_WITHIN(delta, expected, actual) \
+    TEST_ASSERT_TRUE(((uint32_t)(actual) >= ((uint32_t)(expected) - (uint32_t)(delta))) && \
+                     ((uint32_t)(actual) <= ((uint32_t)(expected) + (uint32_t)(delta))))
+
 /* Standard Bosch Datasheet Trimming Test Vectors (BST-BME280-DS002-15 Appendix 8.1) */
 #define VECTOR_DIG_T1       27504U
```

---

## 4. Verification & Prevention Guidelines

1. **Unity Assertion Macro Audit**: Consult [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h) before using compound assertion macros to verify exact symbol availability.
2. **Local Test Helper Standards**: When checking integer delta bounds, either define standardized `TEST_ASSERT_<TYPE>_WITHIN` macros with explicit 32-bit cast safety or use direct `TEST_ASSERT_TRUE(val >= (exp - d) && val <= (exp + d))` bounds checks.
