# Error Report: ERR-028 - Rain Gauge Peak Rate State Mutation on Read Queries

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-028` |
| **Date & Time** | 2026-09-14 11:48:21 +05:30 |
| **Commit SHA** | [`080e1e0`](https://github.com/Thejus26/rain-predict/commit/080e1e0e4687c3039a2a841c6675df5c2c5ef89a) |
| **Component / Subsystem** | Drivers & Metrics (Rain Gauge) |
| **Sprint & Task** | Sprint 4 (`S4-T3.3` & `S4-T3.4`) |
| **Severity** | High (CI Host Test Runner Failure in `test_rain_gauge`) |
| **Impacted Files** | [`firmware/drivers/inc/rain_gauge_driver.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/rain_gauge_driver.h)<br>[`firmware/drivers/src/rain_gauge_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c)<br>[`tests/unit/test_rain_gauge.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_gauge.c) |

---

## 1. Description & Symptoms

During automated CTest execution in the CI runner (`Run ctest --test-dir build-host --output-on-failure`), Test #23 (`rain_gauge`) failed in `test_rain_rate_peak_tracking_and_reset`:

```text
      Start 23: rain_gauge
23/23 Test #23: rain_gauge .......................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:604:test_rain_gauge_macro_definitions:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:605:test_rain_gauge_init_default_state:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:606:test_rain_gauge_first_valid_pulse:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:607:test_rain_gauge_bounce_rejection_10ms:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:608:test_rain_gauge_cumulative_bounce_rejection:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:609:test_rain_gauge_second_valid_pulse:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:610:test_rain_gauge_exact_50ms_boundary:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:611:test_rain_gauge_exact_49ms_boundary:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:612:test_rain_gauge_systick_rollover:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:613:test_rain_gauge_diagnostic_reset:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:614:test_rain_gauge_deinit_and_irq_control:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:617:test_rain_gauge_atomic_read_and_clear_interval:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:618:test_rain_gauge_calibration_math:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:619:test_rain_gauge_rolling_hourly_fifo:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:620:test_rain_gauge_hourly_fifo_overwrite:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:621:test_rain_gauge_daily_persistence_and_reset:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:622:test_rain_gauge_telemetry_byte8_encoding:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:623:test_rain_gauge_get_accumulation_null_guard:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:624:test_rain_gauge_reset_all_accumulators:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:627:test_rain_rate_first_tip_initialization:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:628:test_rain_rate_moderate_rain_inter_tip:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:629:test_rain_rate_cloudburst_extreme:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:630:test_rain_rate_debounce_limit_clamping:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:631:test_rain_rate_time_decay_aging:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:632:test_rain_rate_inactivity_timeout:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:633:test_rain_rate_windowed_interval_rates:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:634:test_rain_rate_intensity_classification_and_strings:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:635:test_rain_rate_sampling_acceleration_trigger:PASS
/home/runner/work/rain-predict/rain-predict/tests/unit/test_rain_gauge.c:592:test_rain_rate_peak_tracking_and_reset:FAIL: Expected 0.000000 Was 20.000000

-----------------------
29 Tests 1 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

In [`firmware/drivers/src/rain_gauge_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c), the getter functions [`rain_gauge_get_instantaneous_rate()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c#L307-L329) and [`rain_gauge_compute_interval_rate()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c#L331-L345) contained mutating side effects that modified internal state variables `s_peak_instantaneous_mm_hr` and `s_peak_interval_mm_hr`:

```c
/* In rain_gauge_get_instantaneous_rate() */
if (base_rate > s_peak_instantaneous_mm_hr) {
    s_peak_instantaneous_mm_hr = base_rate;
}

/* In rain_gauge_compute_interval_rate() */
if (rate > s_peak_interval_mm_hr) {
    s_peak_interval_mm_hr = rate;
}
```

In `tests/unit/test_rain_gauge.c`:
1. `test_rain_rate_peak_tracking_and_reset()` called `rain_gauge_reset_peak_rates()`, which correctly cleared `s_peak_instantaneous_mm_hr = 0.0f` and `s_peak_interval_mm_hr = 0.0f`.
2. The test then called `rain_gauge_get_rate_metrics(134000U, 600U, &metrics)` to verify the reset state.
3. Inside [`rain_gauge_get_rate_metrics()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c#L382-L416), `rain_gauge_get_instantaneous_rate(current_tick_ms)` was called to compute current rate metrics. Because `base_rate` (20.00 mm/hr) was greater than the newly reset `s_peak_instantaneous_mm_hr` (0.00 mm/hr), the getter immediately mutated `s_peak_instantaneous_mm_hr` back to `20.0f`.
4. The subsequent assertion `TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.00f, metrics.peak_instantaneous_mm_hr)` failed with `Expected 0.000000 Was 20.000000`.

Peak rate updates belong to event ingestion (ISR pulse reception and interval completion), not passive data retrieval / calculation getters.

---

## 3. Resolution & Code Changes

1. **Remove Side Effects from Getters / Calculators**:
   - Removed state mutation from [`rain_gauge_get_instantaneous_rate()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c) and [`rain_gauge_compute_interval_rate()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c). These functions now operate as pure mathematical evaluators.
2. **Move Peak Tracking to Ingestion Events**:
   - In [`rain_gauge_exti_isr()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c#L140-L190), evaluate and update `s_peak_instantaneous_mm_hr` whenever a valid bucket tip occurs.
   - In [`rain_gauge_update_hourly_history()`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c#L210-L225), evaluate and update `s_peak_interval_mm_hr` when a sample interval is committed.
3. **Define Default Interval Duration**:
   - Added `#define RAIN_GAUGE_INTERVAL_DURATION_SEC 600U` in [`firmware/drivers/inc/rain_gauge_driver.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/inc/rain_gauge_driver.h).

### Proposed Unified Diff

```diff
diff --git a/firmware/drivers/inc/rain_gauge_driver.h b/firmware/drivers/inc/rain_gauge_driver.h
index 05c8856..d34784f 100644
--- a/firmware/drivers/inc/rain_gauge_driver.h
+++ b/firmware/drivers/inc/rain_gauge_driver.h
@@ -39,6 +39,9 @@ extern "C" {
 /** @brief Number of 10-minute intervals in a rolling 1-hour FIFO (6 * 10 min = 60 min) */
 #define RAIN_GAUGE_HOURLY_FIFO_SIZE         6U
 
+/** @brief Default sample interval duration in seconds (10 minutes) */
+#define RAIN_GAUGE_INTERVAL_DURATION_SEC    600U
+
 /** @brief Maximum tip count representable in 8-bit unsigned LoRaWAN Byte 8 (51.0 mm) */
 #define RAIN_GAUGE_TELEMETRY_MAX_TIPS       255U
 
diff --git a/firmware/drivers/src/rain_gauge_driver.c b/firmware/drivers/src/rain_gauge_driver.c
index 357017f..621de8c 100644
--- a/firmware/drivers/src/rain_gauge_driver.c
+++ b/firmware/drivers/src/rain_gauge_driver.c
@@ -177,6 +177,15 @@ void rain_gauge_exti_isr(uint32_t current_tick_ms) {
         if (s_total_lifetime_tips < 0xFFFFFFFFUL) {
             s_total_lifetime_tips++;
         }
+
+        /* Calculate and update peak instantaneous rate upon valid tip event */
+        float tip_rate = RAIN_RATE_NUMERATOR_MS / (float)elapsed_ms;
+        if (tip_rate > RAIN_RATE_MAX_MM_HR) {
+            tip_rate = RAIN_RATE_MAX_MM_HR;
+        }
+        if (tip_rate > s_peak_instantaneous_mm_hr) {
+            s_peak_instantaneous_mm_hr = tip_rate;
+        }
     } else {
         /* Spurious contact bounce / chatter spike (< 50ms) rejected */
         s_rejected_bounce_count++;
@@ -206,6 +215,15 @@ uint16_t rain_gauge_read_and_clear_interval(float *p_interval_mm) {
 void rain_gauge_update_hourly_history(uint16_t interval_tips) {
     s_hourly_fifo[s_hourly_fifo_index] = interval_tips;
     s_hourly_fifo_index = (uint8_t)((s_hourly_fifo_index + 1U) % RAIN_GAUGE_HOURLY_FIFO_SIZE);
+
+    /* Update peak interval rate on completed interval */
+    float rate = ((float)interval_tips * RAIN_RATE_NUMERATOR_SEC) / (float)RAIN_GAUGE_INTERVAL_DURATION_SEC;
+    if (rate > RAIN_RATE_MAX_MM_HR) {
+        rate = RAIN_RATE_MAX_MM_HR;
+    }
+    if (rate > s_peak_interval_mm_hr) {
+        s_peak_interval_mm_hr = rate;
+    }
 }
 
 uint8_t rain_gauge_encode_telemetry_byte(uint16_t interval_tips) {
@@ -307,11 +325,6 @@ float rain_gauge_get_instantaneous_rate(uint32_t current_tick_ms) {
         base_rate = 0.0f;
     }
 
-    /* Update peak instantaneous rate */
-    if (base_rate > s_peak_instantaneous_mm_hr) {
-        s_peak_instantaneous_mm_hr = base_rate;
-    }
-
     return base_rate;
 }
 
@@ -328,11 +341,6 @@ float rain_gauge_compute_interval_rate(uint16_t interval_tips, uint32_t interval
         rate = 0.0f;
     }
 
-    /* Update peak interval rate */
-    if (rate > s_peak_interval_mm_hr) {
-        s_peak_interval_mm_hr = rate;
-    }
-
     return rate;
 }
```

---

## 4. Verification & Prevention Guidelines

- **Command-Query Separation (CQS)**: Getter and computational conversion functions must never mutate state or side-effect module memory. All state transitions and peak tracking must be driven strictly by explicit event triggers (ISR handlers, interval commitment routines, or explicit reset functions).
- **Reset Idempotence**: Reset functions (`rain_gauge_reset_peak_rates()`, `rain_gauge_reset_diagnostics()`, etc.) must be verified by immediately executing query routines to ensure that subsequent reads do not spontaneously reconstitute cleared state.
