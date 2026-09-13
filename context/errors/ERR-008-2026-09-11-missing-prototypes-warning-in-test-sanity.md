# Error Report: ERR-008 - Missing Prototypes Warning in test_sanity.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-008` |
| **Date & Time** | `2026-09-11 15:46:40 +0530` |
| **Commit SHA** | [`cf79673`](https://github.com/Thejus26/rain-predict/commit/cf7967343d9cfc2451a7548a5017376ba83e9613) |
| **Sprint / Task** | Sprint 1 (S1-T2.5 Sanity Test Suite) |
| **Severity** | Medium (Compiler Error under Strict Diagnostics) |
| **Impacted Files** | [`tests/unit/test_sanity.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sanity.c) |

---

## 1. Description & Symptoms

When building the test runner executable for `test_sanity`, the build aborted with compiler diagnostic errors:
```
tests/unit/test_sanity.c:39:6: error: no previous prototype for 'test_sanity_unity_assertions' [-Werror=missing-prototypes]
tests/unit/test_sanity.c:51:6: error: no previous prototype for 'test_sanity_mock_i2c_loopback' [-Werror=missing-prototypes]
tests/unit/test_sanity.c:73:6: error: no previous prototype for 'test_sanity_mock_uart_loopback' [-Werror=missing-prototypes]
tests/unit/test_sanity.c:104:6: error: no previous prototype for 'test_sanity_mock_gpio_and_exti' [-Werror=missing-prototypes]
tests/unit/test_sanity.c:130:6: error: no previous prototype for 'test_sanity_floating_point_math' [-Werror=missing-prototypes]
```

---

## 2. Root Cause Analysis

In `test_sanity.c`, test case functions were declared globally without `static` or prototype headers, triggering the strict compiler warning `-Wmissing-prototypes` which was treated as an error by `-Werror`.

---

## 3. Resolution & Code Changes

Prepended `static` to all test case functions in `tests/unit/test_sanity.c`:

```diff
-void test_sanity_unity_assertions(void) {
+static void test_sanity_unity_assertions(void) {
...
-void test_sanity_mock_i2c_loopback(void) {
+static void test_sanity_mock_i2c_loopback(void) {
...
-void test_sanity_floating_point_math(void) {
+static void test_sanity_floating_point_math(void) {
```

---

## 4. Verification & Prevention Guidelines

- Enforce standard test templates where all unit test functions are marked `static`.
- Maintain `setUp()` and `tearDown()` as Unity hooks and keep internal test functions static to the compilation unit.
