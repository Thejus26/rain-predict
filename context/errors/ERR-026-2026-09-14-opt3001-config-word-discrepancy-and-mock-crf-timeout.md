# Error Report: ERR-026 - OPT3001 Config Word Discrepancy & Mock CRF Single-Shot Timeout in test_opt3001

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-026` |
| **Date & Time** | 2026-09-14 10:09:00 +05:30 |
| **Commit SHA** | [`82bc551`](https://github.com/Thejus26/rain-predict/commit/82bc55173ec00d8806d6acec27bbaac17b50bbcc) |
| **Component / Subsystem** | Testing & Driver Simulation (OPT3001) |
| **Sprint & Task** | Sprint 4: `S4-T2.1` (TI OPT3001 Single-Shot Driver) |
| **Severity** | High (CI Host Test Runner Failure) |
| **Impacted Files** | [`tests/unit/test_opt3001.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_opt3001.c)<br>[`firmware/drivers/inc/opt3001_driver.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/opt3001_driver.h)<br>[`firmware/drivers/src/i2c_bus.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/i2c_bus.c) |

---

## 1. Description & Symptoms

During automated CTest execution in the CI workflow runner (`Run ctest --test-dir build-host --output-on-failure`), Test #22 (`opt3001`) failed with 2 assertion failures:

```
      Start 22: opt3001
22/22 Test #22: opt3001 ..........................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:45:test_opt3001_definitions_and_constants:FAIL: Expected 0xCA10 Was 0xC210
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:223:test_opt3001_null_pointer_defensive_guards:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:224:test_opt3001_valid_initialization:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:225:test_opt3001_invalid_manufacturer_id_rejection:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:226:test_opt3001_invalid_device_id_rejection:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:227:test_opt3001_trigger_single_shot:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:228:test_opt3001_is_conversion_ready_active:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:229:test_opt3001_is_conversion_ready_complete:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:230:test_opt3001_wait_for_completion_timeout:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_opt3001.c:213:test_opt3001_read_raw_result_and_sample_forced_raw:FAIL: Expected 0 Was 2

-----------------------
10 Tests 2 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

Investigation identified two distinct root causes:

### Root Cause 1: 100ms Single-Shot Configuration Word Value Discrepancy
In the TI OPT3001 configuration register `0x01`:
- `RN[3:0] = 0b1100` (Auto-Range) $\to \mathtt{0xC000}$
- `CT = 0b0` ($100\text{ ms}$ Conversion Time) $\to \mathtt{0x0000}$ (whereas `CT = 0b1` is $800\text{ ms} = \mathtt{0x0800}$)
- `M[1:0] = 0b01` (Single-Shot Mode) $\to \mathtt{0x0200}$
- `L = 0b1` (Latch Window) $\to \mathtt{0x0010}$

Combining these bits gives:
$$\text{CONFIG} = \mathtt{0xC000} \mid \mathtt{0x0000} \mid \mathtt{0x0200} \mid \mathtt{0x0010} = \mathbf{0xC210}$$

However, in `tests/unit/test_opt3001.c`, line 45 asserted that `OPT3001_CONFIG_SINGLE_SHOT_CMD` equals `0xCA10` (which corresponds to $800\text{ ms}$ integration time with `CT = 1`, i.e., $\mathtt{0xC000} \mid \mathtt{0x0800} \mid \mathtt{0x0200} \mid \mathtt{0x0010} = \mathtt{0xCA10}$). The header macro `OPT3001_CONFIG_SINGLE_SHOT_CMD` correctly computed `0xC210` for $100\text{ ms}$, causing the constant unit test assertion mismatch.

### Root Cause 2: Mock Configuration Register Write Overwriting Pre-Set CRF Flag
In `test_opt3001_read_raw_result_and_sample_forced_raw()`, the unit test configured the mock register before calling `opt3001_sample_forced_raw(&dev, &sample)`:
```c
(void)mock_i2c_set_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG, 0xC280U | OPT3001_CONFIG_CRF_BIT);
```
However, inside `opt3001_sample_forced_raw()`, step 1 executes `opt3001_trigger_single_shot(dev)`. This issues an `i2c_bus_write16()` that transmits `OPT3001_CONFIG_SINGLE_SHOT_CMD` (`0xC210`), which overwrites the mock register with `CRF = 0` (bit 7 cleared).

When step 2 (`opt3001_wait_for_completion(dev, 150)`) immediately executed, the simulated register had `CRF = 0` and remained 0 indefinitely because no asynchronous hardware state machine was advancing conversion status in the mock bus. As a result, `wait_for_completion` timed out after 150 ms and returned `STATUS_ERR_TIMEOUT` (value `2`), failing the `TEST_ASSERT_EQUAL_INT(STATUS_OK, status)` check (`Expected 0 Was 2`).

---

## 3. Resolution & Code Changes

1. **Fixed `test_opt3001.c` Constant Assertion & Trigger Test**:
   - Corrected `test_opt3001_definitions_and_constants` to expect `0xC210U` for `OPT3001_CONFIG_SINGLE_SHOT_CMD` ($100\text{ ms}$ auto-range single-shot).
   - Updated `test_opt3001_trigger_single_shot` to assert that `(written_config & ~OPT3001_CONFIG_CRF_BIT) == OPT3001_CONFIG_SINGLE_SHOT_CMD` and that the auto-asserted `CRF` flag is set.
2. **Auto-Assert CRF Flag on Mock Single-Shot Trigger Writes**:
   - In `firmware/drivers/src/i2c_bus.c` (`i2c_bus_write`), when 2 bytes are written to register `0x01` on OPT3001 device addresses (`0x44..0x47`) in single-shot mode (`M[1:0] == 0b01`), automatically set the `CRF` bit (bit 7) so subsequent completion polling succeeds immediately without blocking.

```diff
--- a/firmware/drivers/src/i2c_bus.c
+++ b/firmware/drivers/src/i2c_bus.c
@@ -550,7 +550,15 @@ status_t i2c_bus_write(uint8_t dev_addr,
     p_dev->last_reg = reg_addr;
 
     if (length == 2U) {
-        p_dev->word_registers[reg_addr] = (uint16_t)(((uint16_t)p_data[0] << 8) | (uint16_t)p_data[1]);
+        uint16_t word_val = (uint16_t)(((uint16_t)p_data[0] << 8) | (uint16_t)p_data[1]);
+        /* For OPT3001 in single-shot mode (M[1:0] = 0b01 at bits 10:9), auto-assert CRF (bit 7) for simulated completion */
+        if (dev_addr >= 0x44U && dev_addr <= 0x47U && reg_addr == 0x01U) {
+            uint16_t mode = (word_val >> 9) & 0x03U;
+            if (mode == 0x01U) {
+                word_val |= (1U << 7); /* Assert CRF bit */
+            }
+        }
+        p_dev->word_registers[reg_addr] = word_val;
         p_dev->word_reg_valid[reg_addr] = true;
     }

--- a/tests/unit/test_opt3001.c
+++ b/tests/unit/test_opt3001.c
@@ -42,7 +42,7 @@ static void test_opt3001_definitions_and_constants(void) {
     TEST_ASSERT_EQUAL_HEX16(0x5449U, OPT3001_EXPECTED_MFG_ID);
     TEST_ASSERT_EQUAL_HEX16(0x3001U, OPT3001_EXPECTED_DEV_ID);
-    TEST_ASSERT_EQUAL_HEX16(0xCA10U, OPT3001_CONFIG_SINGLE_SHOT_CMD);
+    TEST_ASSERT_EQUAL_HEX16(0xC210U, OPT3001_CONFIG_SINGLE_SHOT_CMD);
 }
@@ -134,7 +134,8 @@ static void test_opt3001_trigger_single_shot(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_OK, status);
 
     uint16_t written_config = mock_i2c_get_word_register(TEST_OPT3001_ADDR, OPT3001_REG_CONFIG);
-    TEST_ASSERT_EQUAL_HEX16(OPT3001_CONFIG_SINGLE_SHOT_CMD, written_config);
+    TEST_ASSERT_EQUAL_HEX16(OPT3001_CONFIG_SINGLE_SHOT_CMD, written_config & ~OPT3001_CONFIG_CRF_BIT);
+    TEST_ASSERT_TRUE((written_config & OPT3001_CONFIG_CRF_BIT) != 0U);
 }
```

---

## 4. Verification & Prevention Guidelines

- **Analytical Bitfield Verifications**: Manually verify binary composition of command words against register tables prior to writing unit test constant assertions.
- **Mock State Transitions**: Ensure mock drivers emulate hardware state transitions (such as auto-asserting ready flags upon single-shot trigger writes in host simulation mode) to prevent synchronous unit tests from blocking indefinitely.

