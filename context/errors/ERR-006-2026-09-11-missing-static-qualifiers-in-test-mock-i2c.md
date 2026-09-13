# Error Report: ERR-006 - Missing static Qualifiers in test_mock_i2c.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-006` |
| **Date & Time** | `2026-09-11 14:30:33 +0530` |
| **Commit SHA** | [`c5296ab`](https://github.com/Thejus26/rain-predict/commit/c5296abba28d13fd8abd77641f7ded7d6e78b937) |
| **Sprint / Task** | Sprint 1 (S1-T2.2 Mock I2C Bus Driver Tests) |
| **Severity** | Medium (Compiler Error under Strict Diagnostics) |
| **Impacted Files** | [`tests/unit/test_mock_i2c.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_mock_i2c.c) |

---

## 1. Description & Symptoms

Building the unit test target `test_mock_i2c` failed with multiple `-Werror=missing-prototypes` errors:
```
tests/unit/test_mock_i2c.c:45:6: error: no previous prototype for 'test_mock_i2c_basic_read_write' [-Werror=missing-prototypes]
tests/unit/test_mock_i2c.c:62:6: error: no previous prototype for 'test_mock_i2c_sequential_read' [-Werror=missing-prototypes]
...
```

---

## 2. Root Cause Analysis

The project enforces `-Wmissing-prototypes -Werror` to prevent functions with external linkage from lacking prototype declarations in headers. Unity test case functions in `test_mock_i2c.c` were defined without the `static` storage class specifier, giving them external linkage without corresponding header declarations.

---

## 3. Resolution & Code Changes

Added the `static` keyword to all test functions in `tests/unit/test_mock_i2c.c`:

```diff
-void test_mock_i2c_basic_read_write(void) {
+static void test_mock_i2c_basic_read_write(void) {
...
-void test_mock_i2c_null_ptr_guard(void) {
+static void test_mock_i2c_null_ptr_guard(void) {
```

---

## 4. Verification & Prevention Guidelines

- All Unity test case functions within test translation units must be declared `static void test_*(void)`.
- Global function declarations must always have an associated prototype in an included header file.
