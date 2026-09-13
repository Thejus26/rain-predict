# Error Report: ERR-018 - Non-Existent Unity Assertion Macro in test_i2c_bus.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-018` |
| **Date & Time** | `2026-09-13 13:01:21 +0530` |
| **Commit SHA** | [`291debb`](https://github.com/Thejus26/rain-predict/commit/291debbfe36ca6c90130d1cbd9ac3230d465a4f1) |
| **Sprint / Task** | Sprint 3 (S3-T2.1 Bounded I2C Bus Driver Tests) |
| **Severity** | High (Compilation Diagnostic Failure) |
| **Impacted Files** | [`tests/unit/test_i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_i2c_bus.c) |

---

## 1. Description & Symptoms

Compiling `test_i2c_bus` failed with an implicit function / macro error:
```
tests/unit/test_i2c_bus.c:165:5: error: implicit declaration of function 'TEST_ASSERT_INT_WITHIN' [-Werror=implicit-function-declaration]
```

---

## 2. Root Cause Analysis

The test attempted to assert that the 9-clock I2C bus lockup recovery routine toggled between 9 and 18 clock pulses using an invalid macro name `TEST_ASSERT_INT_WITHIN()`. In Unity, floating-point numbers support `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)`, but integer bounds checks must use `TEST_ASSERT_TRUE(val >= min && val <= max)` or `TEST_ASSERT_INT_WITHIN` with proper Unity argument conventions.

---

## 3. Resolution & Code Changes

Replaced the invalid macro with an explicit boolean assertion:

```diff
-    TEST_ASSERT_INT_WITHIN(9, 18, pulse_count);
+    TEST_ASSERT_TRUE(pulse_count >= 9 && pulse_count <= 18);
```

---

## 4. Verification & Prevention Guidelines

- Use `TEST_ASSERT_TRUE` with explicit boolean range operators for integer boundary and interval validation.
- Test build clean execution locally before staging test files.
