# Error Report: ERR-027 - Mock I2C Over-Aggressive CRF Bit Mutation in test_i2c_bus

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-027` |
| **Date & Time** | 2026-09-14 10:16:00 +05:30 |
| **Commit SHA** | [`e92b8d0`](https://github.com/Thejus26/rain-predict/commit/e92b8d0c2d7b126edc107fefb67bea65dd5a6dcc) |
| **Component / Subsystem** | Testing & Driver Simulation (I2C Bus) |
| **Sprint & Task** | Sprint 3 (`S3-T2.1`) & Sprint 4 (`S4-T2.1`) |
| **Severity** | High (CI Host Test Runner Failure in `test_i2c_bus`) |
| **Impacted Files** | [`firmware/drivers/inc/i2c_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/i2c_bus.h)<br>[`firmware/drivers/src/i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/i2c_bus.c)<br>[`tests/mocks/mock_i2c_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/mocks/mock_i2c_bus.h)<br>[`tests/mocks/mock_i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/mocks/mock_i2c_bus.c)<br>[`tests/unit/test_opt3001.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_opt3001.c) |

---

## 1. Description & Symptoms

During automated CTest execution in the CI runner (`Run ctest --test-dir build-host --output-on-failure`), Test #14 (`i2c_bus`) failed in `test_i2c_bus_16bit_word_endianness`:

```
      Start 14: i2c_bus
14/22 Test #14: i2c_bus ..........................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:198:test_i2c_bus_init_speeds:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:199:test_i2c_bus_burst_readback:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:200:test_i2c_bus_write_sequence:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:115:test_i2c_bus_16bit_word_endianness:FAIL: Expected 0x1234 Was 0x12B4
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:202:test_i2c_bus_probe_and_nack:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:203:test_i2c_bus_9clock_lockup_recovery:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:204:test_i2c_bus_defensive_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_i2c_bus.c:205:test_i2c_bus_deinit_and_busy:PASS

-----------------------
8 Tests 1 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

In the fix for `ERR-026`, logic was added to `i2c_bus_write()` in `firmware/drivers/src/i2c_bus.c` to auto-assert the `CRF` bit (bit 7) whenever a 16-bit write occurred to register `0x01` on I2C addresses `0x44..0x47` if bits [10:9] (`mode`) equaled `0b01`:

```c
if (dev_addr >= 0x44U && dev_addr <= 0x47U && reg_addr == 0x01U) {
    uint16_t mode = (word_val >> 9) & 0x03U;
    if (mode == 0x01U) {
        word_val |= (1U << 7); /* Assert CRF bit */
    }
}
```

In `tests/unit/test_i2c_bus.c`, the generic 16-bit endianness test `test_i2c_bus_16bit_word_endianness()` writes the arbitrary test pattern `0x1234` to `TEST_OPT3001_ADDR` (`0x44`) at `TEST_REG_CONFIG` (`0x01`).

In the test pattern `0x1234` (`0b0001_0010_0011_0100`):
- Bits [10:9] are `0b01` (`(0x1234 >> 9) & 0x03 == 1`).
- As a result, the simulated driver unintentionally detected `mode == 0x01` and mutated bit 7 (`0x80`), transforming the lower byte from `0x34` (`0b0011_0100`) to `0xB4` (`0b1011_0100`), yielding `0x12B4`.

When `test_i2c_bus_16bit_word_endianness()` read back the register word, it received `0x12B4` instead of the written `0x1234`, failing the test.

---

## 3. Resolution & Code Changes

1. **Decoupled Simulation Feature Flag**:
   - Added `s_sim_opt3001_auto_crf` (boolean, default `false`) in [`firmware/drivers/src/i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/i2c_bus.c).
   - Added public control function `i2c_bus_test_set_opt3001_auto_crf(bool enable)` in [`firmware/drivers/inc/i2c_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/i2c_bus.h) and [`tests/mocks/mock_i2c_bus.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/mocks/mock_i2c_bus.h) / [`tests/mocks/mock_i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/mocks/mock_i2c_bus.c).
   - Reset `s_sim_opt3001_auto_crf = false` on every test reset in `i2c_bus_test_reset()`.
2. **Guarded Bus Mutation in `i2c_bus_write()`**:
   - In `i2c_bus_write()`, the auto-assertion of CRF is guarded by `if (s_sim_opt3001_auto_crf && ...)` so generic I2C bus tests remain 100% transparent.
3. **Explicit Mock Configuration in OPT3001 Tests**:
   - Updated `setup_valid_mock_opt3001()` in [`tests/unit/test_opt3001.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_opt3001.c) to call `mock_i2c_set_auto_crf(true)`.
   - Updated `test_opt3001_wait_for_completion_timeout()` to explicitly call `mock_i2c_set_auto_crf(false)`.

### Unified Diff

```diff
diff --git a/firmware/drivers/inc/i2c_bus.h b/firmware/drivers/inc/i2c_bus.h
index ad3910d..cddc83a 100644
--- a/firmware/drivers/inc/i2c_bus.h
+++ b/firmware/drivers/inc/i2c_bus.h
@@ -261,6 +261,12 @@ status_t i2c_bus_test_set_slave_word_reg(uint8_t dev_addr, uint8_t reg_addr, uin
  */
 uint16_t i2c_bus_test_get_slave_word_reg(uint8_t dev_addr, uint8_t reg_addr);
 
+/**
+ * @brief Enables or disables simulated auto-assert of CRF flag for OPT3001 single-shot conversion writes.
+ * @param[in] enable true to enable simulated conversion completion.
+ */
+void i2c_bus_test_set_opt3001_auto_crf(bool enable);
+
 #endif /* !HAVE_STM32WLXX_HAL */
 
 #ifdef __cplusplus
diff --git a/firmware/drivers/src/i2c_bus.c b/firmware/drivers/src/i2c_bus.c
index 9cd1b0f..db01f8c 100644
--- a/firmware/drivers/src/i2c_bus.c
+++ b/firmware/drivers/src/i2c_bus.c
@@ -264,6 +264,7 @@ static status_t s_sim_injected_fault = STATUS_OK;
 static uint32_t s_sim_fault_trigger_delay = 0;
 static uint32_t s_sim_transaction_count = 0;
 static uint32_t s_sim_active_fault_type = 0;
+static bool s_sim_opt3001_auto_crf = false;
 
 static sim_i2c_device_t *sim_find_device(uint8_t dev_addr) {
     for (size_t i = 0; i < SIM_I2C_MAX_DEVICES; i++) {
@@ -300,6 +301,7 @@ void i2c_bus_test_reset(void) {
     s_sim_fault_trigger_delay = 0;
     s_sim_transaction_count = 0;
     s_sim_active_fault_type = 0;
+    s_sim_opt3001_auto_crf = false;
 }
 
 void i2c_bus_test_set_sda_stuck(bool stuck_low) {
@@ -409,6 +411,10 @@ uint16_t i2c_bus_test_get_slave_word_reg(uint8_t dev_addr, uint8_t reg_addr) {
     return 0x0000U;
 }
 
+void i2c_bus_test_set_opt3001_auto_crf(bool enable) {
+    s_sim_opt3001_auto_crf = enable;
+}
+
 status_t i2c_bus_init(uint32_t speed_hz) {
     if (speed_hz != I2C_BUS_SPEED_STANDARD_HZ && speed_hz != I2C_BUS_SPEED_FAST_HZ) {
         return STATUS_ERR_INVALID_PARAM;
@@ -551,8 +557,8 @@ status_t i2c_bus_write(uint8_t dev_addr,
 
     if (length == 2U) {
         uint16_t word_val = (uint16_t)(((uint16_t)p_data[0] << 8) | (uint16_t)p_data[1]);
-        /* For OPT3001 in single-shot mode (M[1:0] = 0b01 at bits 10:9), auto-assert CRF (bit 7) for simulated completion */
-        if (dev_addr >= 0x44U && dev_addr <= 0x47U && reg_addr == 0x01U) {
+        /* For OPT3001 in single-shot mode (M[1:0] = 0b01 at bits 10:9), auto-assert CRF (bit 7) if enabled */
+        if (s_sim_opt3001_auto_crf && dev_addr >= 0x44U && dev_addr <= 0x47U && reg_addr == 0x01U) {
             uint16_t mode = (word_val >> 9) & 0x03U;
             if (mode == 0x01U) {
                 word_val |= (1U << 7); /* Assert CRF bit */
diff --git a/tests/mocks/mock_i2c_bus.c b/tests/mocks/mock_i2c_bus.c
index 0e25aaa..bd01d43 100644
--- a/tests/mocks/mock_i2c_bus.c
+++ b/tests/mocks/mock_i2c_bus.c
@@ -36,6 +36,10 @@ uint16_t mock_i2c_get_word_register(uint8_t dev_addr, uint8_t reg_addr) {
     return i2c_bus_test_get_slave_word_reg(dev_addr, reg_addr);
 }
 
+void mock_i2c_set_auto_crf(bool enable) {
+    i2c_bus_test_set_opt3001_auto_crf(enable);
+}
+
 void mock_i2c_inject_fault(mock_i2c_fault_t fault, uint32_t trigger_after_n_calls) {
     i2c_bus_test_inject_fault_advanced((uint32_t)fault, trigger_after_n_calls);
 }
diff --git a/tests/mocks/mock_i2c_bus.h b/tests/mocks/mock_i2c_bus.h
index 8802ed0..ce72e06 100644
--- a/tests/mocks/mock_i2c_bus.h
+++ b/tests/mocks/mock_i2c_bus.h
@@ -42,6 +42,7 @@ status_t mock_i2c_set_registers(uint8_t dev_addr, uint8_t start_reg, const uint8
 uint8_t  mock_i2c_get_register(uint8_t dev_addr, uint8_t reg_addr);
 status_t mock_i2c_set_word_register(uint8_t dev_addr, uint8_t reg_addr, uint16_t value);
 uint16_t mock_i2c_get_word_register(uint8_t dev_addr, uint8_t reg_addr);
+void     mock_i2c_set_auto_crf(bool enable);
 
 void mock_i2c_inject_fault(mock_i2c_fault_t fault, uint32_t trigger_after_n_calls);
 void mock_i2c_clear_faults(void);
diff --git a/tests/unit/test_opt3001.c b/tests/unit/test_opt3001.c
index a3308f6..91eac35 100644
--- a/tests/unit/test_opt3001.c
+++ b/tests/unit/test_opt3001.c
@@ -24,6 +24,7 @@ void tearDown(void) {
  * @brief Helper to set up standard valid OPT3001 mock IDs.
  */
 static void setup_valid_mock_opt3001(uint8_t addr) {
+    mock_i2c_set_auto_crf(true);
     (void)mock_i2c_set_word_register(addr, OPT3001_REG_MANUFACTURER_ID, OPT3001_EXPECTED_MFG_ID);
     (void)mock_i2c_set_word_register(addr, OPT3001_REG_DEVICE_ID, OPT3001_EXPECTED_DEV_ID);
 }
@@ -180,6 +181,7 @@ static void test_opt3001_wait_for_completion_timeout(void) {
     setup_valid_mock_opt3001(TEST_OPT3001_ADDR);
     TEST_ASSERT_EQUAL_INT(STATUS_OK, opt3001_init(&dev, TEST_OPT3001_ADDR));
 
+    mock_i2c_set_auto_crf(false);
     /* CRF continuously 0 */
     (void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC210U);
```

---

## 4. Verification & Prevention Guidelines

- **Clean Bus Layer Abstraction**: Bus-level mock drivers (`i2c_bus.c`) must remain generic, transparent emulators and never contain sensor-specific protocol heuristics or payload mutations. All sensor-specific behaviors should be controlled via explicit test mock configuration APIs.
- **Regression Suite Verification**: Always run the complete test suite (`ctest --output-on-failure`) across all existing drivers whenever modifying shared mock bus infrastructure.
