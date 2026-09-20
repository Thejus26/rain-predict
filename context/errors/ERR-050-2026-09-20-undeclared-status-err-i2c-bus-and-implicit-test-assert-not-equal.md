# Error Report: ERR-050 - Undeclared Identifier 'STATUS_ERR_I2C_BUS' and Non-Existent Unity Macro in test_fault_injection.c

## Metadata

| Attribute | Specification Details |
| :--- | :--- |
| **Error ID** | `ERR-050` |
| **Date & Time** | 2026-09-20 18:58:00+05:30 |
| **Commit SHA** | [`413b76d`](https://github.com/Thejus26/rain-predict/commit/413b76dc74785fc8d002dfdce93ae38afc7e8855) |
| **Sprint / Task** | `Sprint 6 / S6-T4.2 (System Fault Injection & Resilience Integration Tests)` |
| **Severity** | High (Host CI Compilation Failure under `-Werror`) |
| **Impacted Files** | [`tests/integration/test_fault_injection.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_fault_injection.c) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI execution for the host unit & integration test build on Linux (`.github/workflows/ci.yml`), compilation failed when compiling `tests/integration/test_fault_injection.c`:

```text
FAILED: [code=1] tests/CMakeFiles/test_fault_injection.dir/integration/test_fault_injection.c.o 
/usr/bin/cc -DHOST_TEST=1 -DUNITY_INCLUDE_CONFIG_H=1 -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_fault_injection.dir/integration/test_fault_injection.c.o -MF tests/CMakeFiles/test_fault_injection.dir/integration/test_fault_injection.c.o.d -o tests/CMakeFiles/test_fault_injection.dir/integration/test_fault_injection.c.o -c /home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c: In function ‘i2c_bus_recover’:
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c:273:16: error: ‘STATUS_ERR_I2C_BUS’ undeclared (first use in this function); did you mean ‘STATUS_ERR_UART_BUS’?
  273 |         return STATUS_ERR_I2C_BUS;
      |                ^~~~~~~~~~~~~~~~~~
      |                STATUS_ERR_UART_BUS
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c:273:16: note: each undeclared identifier is reported only once for each function it appears in
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c: In function ‘bme280_read_data’:
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c:296:16: error: ‘STATUS_ERR_I2C_BUS’ undeclared (first use in this function); did you mean ‘STATUS_ERR_UART_BUS’?
  296 |         return STATUS_ERR_I2C_BUS;
      |                ^~~~~~~~~~~~~~~~~~
      |                STATUS_ERR_UART_BUS
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c: In function ‘opt3001_read_lux’:
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c:317:16: error: ‘STATUS_ERR_I2C_BUS’ undeclared (first use in this function); did you mean ‘STATUS_ERR_UART_BUS’?
  317 |         return STATUS_ERR_I2C_BUS;
      |                ^~~~~~~~~~~~~~~~~~
      |                STATUS_ERR_UART_BUS
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c: In function ‘test_fi_opt3001_dead_solar_heuristic’:
/home/runner/work/rain-predict/rain-predict/tests/integration/test_fault_injection.c:581:5: error: implicit declaration of function ‘TEST_ASSERT_NOT_EQUAL’; did you mean ‘TEST_ASSERT_NOT_NULL’? [-Werror=implicit-function-declaration]
  581 |     TEST_ASSERT_NOT_EQUAL(RAIN_ALERT_IMMINENT, ctx->rain_state);
      |     ^~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_NOT_NULL
cc1: all warnings being treated as errors
```

---

## 2. Root Cause Analysis

Two distinct diagnostic errors caused the compilation failure under GCC's `-Werror`:

1. **Undeclared Identifier `STATUS_ERR_I2C_BUS`**:
   * In [`tests/integration/test_fault_injection.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_fault_injection.c), mock stubs `i2c_bus_recover`, `bme280_read_data`, and `opt3001_read_lux` returned `STATUS_ERR_I2C_BUS` when fault injection was active.
   * However, in [`firmware/core/inc/status.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/status.h), the canonical enumeration identifier defined in `status_t` is `STATUS_ERR_I2C` (along with `STATUS_ERR_UART_BUS` and `STATUS_ERR_SENSOR_NO_RESPONSE`). The token `STATUS_ERR_I2C_BUS` is not defined anywhere in the codebase.

2. **Non-Existent Unity Macro `TEST_ASSERT_NOT_EQUAL`**:
   * In test function `test_fi_opt3001_dead_solar_heuristic`, line 581 called `TEST_ASSERT_NOT_EQUAL(RAIN_ALERT_IMMINENT, ctx->rain_state);`.
   * The ThrowTheSwitch Unity test framework (`tests/unity/unity.h`) does not define a generic untyped `TEST_ASSERT_NOT_EQUAL` macro. Unlike `TEST_ASSERT_EQUAL(expected, actual)` (which maps to `TEST_ASSERT_EQUAL_INT`), Unity only provides `TEST_ASSERT_TRUE(condition)` or `TEST_ASSERT_FALSE(condition)` for asserting inequality.
   * Calling `TEST_ASSERT_NOT_EQUAL(...)` resulted in an implicit function declaration, which was escalated to a fatal compilation error by `-Werror=implicit-function-declaration`.

---

## 3. Resolution & Code Changes

*Status: Implemented & Verified*

### Implemented Changes in `tests/integration/test_fault_injection.c`:

1. Replaced all occurrences of undeclared `STATUS_ERR_I2C_BUS` with canonical `STATUS_ERR_I2C` in:
   * `i2c_bus_recover()` (line 273).
   * `bme280_read_data()` (line 296).
   * `opt3001_read_lux()` (line 317).

2. Replaced non-existent `TEST_ASSERT_NOT_EQUAL(RAIN_ALERT_IMMINENT, ctx->rain_state)` with standard Unity boolean assertion `TEST_ASSERT_TRUE(ctx->rain_state != RAIN_ALERT_IMMINENT)` in `test_fi_opt3001_dead_solar_heuristic()` (line 581).

### Unified Diff:

```diff
diff --git a/tests/integration/test_fault_injection.c b/tests/integration/test_fault_injection.c
--- a/tests/integration/test_fault_injection.c
+++ b/tests/integration/test_fault_injection.c
@@ -270,7 +270,7 @@ status_t bsp_adc_read_vbat_mv(uint16_t *p_vbat_mv) {
 status_t i2c_bus_recover(void) {
     s_mock.i2c_recovery_calls++;
     if (s_mock.inject_i2c_lockup) {
-        return STATUS_ERR_I2C_BUS;
+        return STATUS_ERR_I2C;
     }
     return STATUS_OK;
 }
@@ -293,7 +293,7 @@ status_t bme280_read_data(bme280_dev_t *dev, bme280_data_t *p_data) {
         return STATUS_ERR_NULL_PTR;
     }
     if (s_mock.inject_bme280_nack || s_mock.inject_i2c_lockup) {
-        return STATUS_ERR_I2C_BUS;
+        return STATUS_ERR_I2C;
     }
     p_data->temperature_c    = s_mock.mock_temp_c;
     p_data->humidity_percent = s_mock.mock_rh_pct;
@@ -314,7 +314,7 @@ status_t opt3001_read_lux(opt3001_dev_t *dev, opt3001_reading_t *p_reading) {
         return STATUS_ERR_NULL_PTR;
     }
     if (s_mock.inject_opt3001_dead || s_mock.inject_i2c_lockup) {
-        return STATUS_ERR_I2C_BUS;
+        return STATUS_ERR_I2C;
     }
     p_reading->lux             = s_mock.mock_lux;
     p_reading->irradiance_w_m2 = s_mock.mock_lux / 120.0f;
@@ -578,7 +578,7 @@ static void test_fi_opt3001_dead_solar_heuristic(void) {
     /* Daytime solar heuristic estimate applied (25000 Lux) */
     TEST_ASSERT_FLOAT_WITHIN(1.0f, FAULT_LUX_DAYTIME_ESTIMATE, ctx->solar_lux);
     /* Rain alert must not false-trigger into IMMINENT due to optical loss */
-    TEST_ASSERT_NOT_EQUAL(RAIN_ALERT_IMMINENT, ctx->rain_state);
+    TEST_ASSERT_TRUE(ctx->rain_state != RAIN_ALERT_IMMINENT);
     TEST_ASSERT_TRUE((s_mock.last_lora_payload[11] & 0x20) != 0U);
     TEST_ASSERT_TRUE(s_mock.in_stop2_sleep);
 }
```

---

## 4. Verification & Prevention Guidelines

1. **Verify Enum Identifiers Against Canonical Core Headers**:
   * Always reference [`firmware/core/inc/status.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/core/inc/status.h) when returning `status_t` error codes in mock stubs instead of assuming naming conventions from other subsystems.

2. **Verify Assertion Macros Against `unity.h`**:
   * Check [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h) for supported macros. Never assume JUnit/PyTest-style assertions (`TEST_ASSERT_NOT_EQUAL`) exist in Unity without checking the header definitions.

3. **CI Compilation Verification**:
   * Validate that all test targets build cleanly with zero compiler warnings under `-Wall -Wextra -Wpedantic -Wstrict-prototypes -Werror`.
