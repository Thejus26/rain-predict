# Error Report: ERR-013 - Missing static Qualifiers and Unused Variable in test_rain_algo.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-013` |
| **Date & Time** | `2026-09-12 14:13:50 +0530` |
| **Commit SHA** | [`63ba9fe`](https://github.com/Thejus26/rain-predict/commit/63ba9fe0c4035c9febde84a71b12b4196b00f918) |
| **Sprint / Task** | Sprint 2 (S2-T5.3 Composite Nowcasting Engine Tests) |
| **Severity** | Medium (Compiler Diagnostic under Strict C99) |
| **Impacted Files** | [`tests/unit/test_rain_algo.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_algo.c) |

---

## 1. Description & Symptoms

Building the `test_rain_algo` test executable failed under `-Werror`:
1. `tests/unit/test_rain_algo.c:18:6: error: no previous prototype for 'test_rain_algo_sub_scores' [-Werror=missing-prototypes]`
2. `tests/unit/test_rain_algo.c:188:11: error: unused variable 'cpi' [-Werror=unused-variable]`

---

## 2. Root Cause Analysis

1. Test functions in `test_rain_algo.c` were declared with global linkage rather than `static`.
2. A temporary variable `float cpi;` was declared in `test_rain_algo_defensive_guards()` but remained unreferenced after test refactoring.

---

## 3. Resolution & Code Changes

Added `static` to all test functions and removed the unused `float cpi;` declaration:

```diff
-void test_rain_algo_sub_scores(void)
+static void test_rain_algo_sub_scores(void)
...
 static void test_rain_algo_defensive_guards(void)
 {
     rain_forecast_t fc;
-    float cpi;
     rain_alert_state_t state;
```

---

## 4. Verification & Prevention Guidelines

- Remove unused local variables promptly when refactoring test routines.
- Maintain consistency with `-Wmissing-prototypes` by marking all translation-unit-local test functions `static`.
