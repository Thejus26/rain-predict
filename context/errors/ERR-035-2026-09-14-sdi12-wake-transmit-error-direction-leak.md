# Error Report: ERR-035 - SDI-12 Wake and Transmit Line Direction State Leak on Invalid Command

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-035` |
| **Date & Time** | 2026-09-14 15:04:55 +05:30 |
| **Commit SHA** | [`2a66eb9`](https://github.com/Thejus26/rain-predict/commit/2a66eb9ffab908c8e844467fb9790ddace138431) |
| **Component / Subsystem** | Driver State & Hardware Simulation (SDI-12 / GPIO) |
| **Sprint & Task** | Sprint 4 (`S4-T5.1` & `S4-T5.3` SDI-12 Bus Driver) |
| **Severity** | High (Unit Test Suite Failure in `test_sdi12`) |
| **Impacted Files** | [`firmware/drivers/src/sdi12_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/sdi12_driver.c)<br>[`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c) |

---

## 1. Description & Symptoms

During CTest test suite execution, `test_sdi12` failed on test case `test_sdi12_wake_and_transmit_null_and_invalid`:

```text
22/25 Test #22: opt3001 ..........................   Passed    0.00 sec
      Start 23: rain_gauge
23/25 Test #23: rain_gauge .......................   Passed    0.00 sec
      Start 24: modbus_rtu
24/25 Test #24: modbus_rtu .......................   Passed    0.00 sec
      Start 25: sdi12
25/25 Test #25: sdi12 ............................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:191:test_sdi12_init_defaults:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:192:test_sdi12_direction_control:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:193:test_sdi12_send_break_and_mark_execution:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:194:test_sdi12_transmit_command_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:195:test_sdi12_transmit_command_missing_exclamation:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:196:test_sdi12_transmit_command_empty_and_overflow:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:197:test_sdi12_wake_and_transmit_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:32:test_sdi12_wake_and_transmit_null_and_invalid:FAIL: Expected 0 Was 1
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:199:test_sdi12_receive_response_valid:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:200:test_sdi12_receive_response_missing_crlf:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:201:test_sdi12_receive_response_timeout:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:202:test_sdi12_receive_response_null_and_overflow:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_sdi12.c:203:test_sdi12_deinit:PASS

-----------------------
13 Tests 1 Failures 0 Ignored
FAIL

Errors while running CTest

96% tests passed, 1 tests failed out of 25

Total Test time (real) =   0.08 sec

The following tests FAILED:
	 25 - sdi12 (Failed)
Error: Process completed with exit code 8.
```

---

## 2. Root Cause Analysis

In [`firmware/drivers/src/sdi12_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/sdi12_driver.c):
1. `sdi12_wake_and_transmit(p_cmd)` calls `sdi12_send_break_and_mark()`, which asserts transmit direction (`PC2 = 1` / `SDI12_DIR_TX`) to execute the physical spacing and marking sequences.
2. `sdi12_wake_and_transmit` then delegates to `sdi12_transmit_command(p_cmd)`.
3. When `p_cmd` is invalid (e.g. `"0D0"` lacking the trailing `'!'`), `sdi12_transmit_command` immediately returns `STATUS_ERROR_INVALID_PARAM` without restoring `sdi12_set_direction(SDI12_DIR_RX)`.
4. As a result, the half-duplex direction line `PC2` is left stuck in `TX` mode (`1`), violating the fail-safe line release requirement and causing the `tearDown()` assertion (`TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2))`) in [`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c) to fail (`Expected 0 Was 1`).

---

## 3. Resolution & Code Changes

### Resolution Steps
1. Updated `sdi12_wake_and_transmit()` in [`firmware/drivers/src/sdi12_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/sdi12_driver.c) to validate command non-null, length within bounds, and trailing `'!'` delimiter before initiating break/mark hardware sequencing.
2. Added error-handling guard around `sdi12_transmit_command()` inside `sdi12_wake_and_transmit()` to unconditionally enforce `sdi12_set_direction(SDI12_DIR_RX)` on any non-OK status.
3. Enhanced `test_sdi12_wake_and_transmit_null_and_invalid` in [`tests/unit/test_sdi12.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_sdi12.c) with direct assertions verifying `PC2 == 0` (RX mode).

### Code Diff
```diff
diff --git a/firmware/drivers/src/sdi12_driver.c b/firmware/drivers/src/sdi12_driver.c
index 1c8b334..5c9762d 100644
--- a/firmware/drivers/src/sdi12_driver.c
+++ b/firmware/drivers/src/sdi12_driver.c
@@ -158,6 +158,11 @@ status_t sdi12_wake_and_transmit(const char *p_cmd) {
         return STATUS_ERROR_NULL_POINTER;
     }
 
+    size_t len = strlen(p_cmd);
+    if (len == 0U || len > (size_t)SDI12_MAX_BUFFER_SIZE || p_cmd[len - 1U] != '!') {
+        return STATUS_ERROR_INVALID_PARAM;
+    }
+
     /* Flush stale RX buffer */
     (void)uart_bus_flush(UART_PORT_SDI12);
 
@@ -165,7 +170,11 @@ status_t sdi12_wake_and_transmit(const char *p_cmd) {
     sdi12_send_break_and_mark();
 
     /* Transmit formatted ASCII command and transition to RX */
-    return sdi12_transmit_command(p_cmd);
+    status_t status = sdi12_transmit_command(p_cmd);
+    if (status != STATUS_OK) {
+        sdi12_set_direction(SDI12_DIR_RX);
+    }
+    return status;
 }
 
 /* ============================================================================
diff --git a/tests/unit/test_sdi12.c b/tests/unit/test_sdi12.c
index 3771f46..3655c1a 100644
--- a/tests/unit/test_sdi12.c
+++ b/tests/unit/test_sdi12.c
@@ -127,7 +127,12 @@ static void test_sdi12_wake_and_transmit_valid(void) {
 
 static void test_sdi12_wake_and_transmit_null_and_invalid(void) {
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_NULL_POINTER, sdi12_wake_and_transmit(NULL));
+    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
+    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
+
     TEST_ASSERT_EQUAL_INT(STATUS_ERROR_INVALID_PARAM, sdi12_wake_and_transmit("0D0"));
+    TEST_ASSERT_EQUAL_INT(0, gpio_read_pin(GPIO_PORT_C, 2));
+    TEST_ASSERT_EQUAL_INT(SDI12_DIR_RX, sdi12_test_get_direction());
 }
 
 static void test_sdi12_receive_response_valid(void) {
```

---

## 4. Verification & Prevention Guidelines

1. **Fail-Safe Half-Duplex Bus Release**:
   - Every physical layer driver function that mutates direction to TX mode must have guaranteed teardown or error-handling blocks that reset the line to RX listening mode.
2. **Early Parameter Validation**:
   - Validate pointers, lengths, and format delimiters before triggering multi-millisecond hardware wakeup sequences.
3. **Unity Teardown Verification**:
   - Retain `tearDown()` invariant assertions that enforce hardware pins return to safe default states after every test run.
