# Error Report: ERR-023 - Stale LOW_BAT_SLEEP_SEC Assertion Mismatch in test_power_mgr.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-023` |
| **Date & Time** | `2026-09-13 17:32:10 +0530` |
| **Commit SHA** | [`dc4c3df`](https://github.com/Thejus26/rain-predict/commit/dc4c3dfd19ce44f7f2f1869bae9884eab024a512) |
| **Sprint / Task** | Sprint 3 (S3-T4.4 Battery ADC Telemetry & S3-T4.2 Low-Power Sleep Manager) |
| **Severity** | Medium (Host Unit Test Regression Failure) |
| **Impacted Files** | [`tests/unit/test_power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_power_mgr.c) |

---

## 1. Description & Symptoms

When running automated host unit tests via CTest (`ctest --test-dir build-host --output-on-failure`), Test #18 (`power_mgr`) failed with a numerical assertion mismatch:

```text
      Start 18: power_mgr
18/20 Test #18: power_mgr ........................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:56:test_power_mgr_init_and_states:FAIL: Expected 1800 Was 900
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:498:test_power_mgr_isolate_sensor_buses:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:499:test_power_mgr_parasitic_leakage_prevention:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:500:test_power_mgr_unused_pin_conditioning:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:501:test_power_mgr_rain_gauge_exti_exemption:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:502:test_power_mgr_gate_and_actuator_exemptions:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:503:test_power_mgr_wake_restore_gpio:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:504:test_power_mgr_verify_leakage_state:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:505:test_power_mgr_rtc_wakeup_config:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:506:test_power_mgr_stop2_cycle_and_metrics:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:507:test_power_mgr_multi_source_wake_reasons:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:508:test_power_mgr_clock_restoration:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_power_mgr.c:509:test_power_mgr_standby_mode:PASS

-----------------------
13 Tests 1 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

In task **S3-T4.2** (Stop 2 Deep Sleep Manager), `POWER_MGR_LOW_BAT_SLEEP_SEC` was initially specified as `1800U` ($30\text{ minutes}$) and tested in [`tests/unit/test_power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_power_mgr.c) using:

```c
TEST_ASSERT_EQUAL_UINT32(1800U, POWER_MGR_LOW_BAT_SLEEP_SEC);
```

Subsequently in task **S3-T4.4** (Battery ADC Voltage Measurement & Solar Telemetry), the multi-tier battery preservation strategy was formally established:
- **Low Battery ($3.00\text{V} \le V_{\text{bat}} < 3.25\text{V}$)**: Sampling throttled to $\ge 15\text{ minutes}$ (`#define POWER_MGR_LOW_BAT_SLEEP_SEC 900U`).
- **Critical Battery ($V_{\text{bat}} < 3.00\text{V}$)**: Emergency preservation sampling of $60\text{ minutes}$ (`#define POWER_MGR_CRITICAL_BAT_SLEEP_SEC 3600U`).

When [`firmware/middleware/inc/power_mgr.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/power_mgr.h) was updated to `900U` for low battery preservation, the assertion in [`tests/unit/test_power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_power_mgr.c) line 56 was left with the stale value `1800U`, triggering a test failure during full regression test execution.

---

## 3. Resolution & Code Changes

Updated [`tests/unit/test_power_mgr.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_power_mgr.c) to validate the updated `POWER_MGR_LOW_BAT_SLEEP_SEC` ($900\text{s}$), added verification for `POWER_MGR_CRITICAL_BAT_SLEEP_SEC` ($3600\text{s}$), `POWER_MGR_MIN_SLEEP_SEC` ($1\text{s}$), and `POWER_MGR_MAX_SLEEP_SEC` ($65535\text{s}$):

```diff
--- a/tests/unit/test_power_mgr.c
+++ b/tests/unit/test_power_mgr.c
@@ -53,7 +53,10 @@ static void test_power_mgr_init_and_states(void) {
     TEST_ASSERT_EQUAL_UINT32(600U, POWER_MGR_DEFAULT_SLEEP_SEC);
     TEST_ASSERT_EQUAL_UINT32(300U, POWER_MGR_WATCH_SLEEP_SEC);
     TEST_ASSERT_EQUAL_UINT32(120U, POWER_MGR_STORM_SLEEP_SEC);
-    TEST_ASSERT_EQUAL_UINT32(1800U, POWER_MGR_LOW_BAT_SLEEP_SEC);
+    TEST_ASSERT_EQUAL_UINT32(900U, POWER_MGR_LOW_BAT_SLEEP_SEC);
+    TEST_ASSERT_EQUAL_UINT32(3600U, POWER_MGR_CRITICAL_BAT_SLEEP_SEC);
+    TEST_ASSERT_EQUAL_UINT32(1U, POWER_MGR_MIN_SLEEP_SEC);
+    TEST_ASSERT_EQUAL_UINT32(65535U, POWER_MGR_MAX_SLEEP_SEC);
 
     /* Verify power manager initialization */
     status_t status = power_mgr_init();
```

---

## 4. Verification & Prevention Guidelines

1. **Full Test Suite Regression**: Always run the complete CTest test suite (`ctest --test-dir build-host --output-on-failure`) when modifying subsystem headers and timing constants.
2. **Cross-Task Synchronization**: When refining constants in downstream tasks (e.g., S3-T4.4 modifying intervals defined in S3-T4.2), ensure all earlier unit test suites referencing those constants are synchronized.
