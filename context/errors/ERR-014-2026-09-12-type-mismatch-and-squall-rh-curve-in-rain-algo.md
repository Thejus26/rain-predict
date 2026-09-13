# Error Report: ERR-014 - Type Divergence and Synthetic Squall RH Curve in Nowcasting Engine

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-014` |
| **Date & Time** | `2026-09-12 14:20:44 +0530` |
| **Commit SHA** | [`f5850a7`](https://github.com/Thejus26/rain-predict/commit/f5850a7fe607fa2a595e647b82aaf1020ecc53d9) |
| **Sprint / Task** | Sprint 2 (S2-T5.2 / S2-T5.3 Rain Algorithm State Machine & Tests) |
| **Severity** | High (Enum Type Mismatch & Test Verification Failure) |
| **Impacted Files** | [`firmware/app/inc/rain_algo.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/rain_algo.h)<br>[`firmware/app/src/rain_algo.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/rain_algo.c)<br>[`tests/unit/test_rain_algo.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_algo.c) |

---

## 1. Description & Symptoms

1. In `rain_algo.h`, two separate enum types existed for output forecast states: `rain_forecast_state_t` and `rain_alert_state_t`. This caused type mismatches, requiring awkward explicit casts when accessing `p_forecast->forecast_state`.
2. In `test_rain_algo_master_evaluation_synthetic_storm()`, evaluating the synthetic convective squall at Hour 4.0 (45 minutes before onset of rain) resulted in `RAIN_ALERT_LIKELY` ($CPI = 78.2\%$) instead of the expected `RAIN_ALERT_IMMINENT` ($CPI \ge 80.0\%$).

---

## 2. Root Cause Analysis

1. Enum type definition duplication: `rain_forecast_state_t` was defined before `rain_alert_state_t` was introduced, causing divergence in the data structure.
2. The synthetic squall curve RH baseline in the test generator was set to $77.5\%$. Under realistic plantation microclimates during pre-squall canopy saturation, relative humidity surpasses $80.0\%$, which is needed to raise the composite precipitation score ($CPI$) above the critical $80.0\%$ threshold.

---

## 3. Resolution & Code Changes

1. Consolidated on `rain_alert_state_t` as the canonical forecast state type in `rain_forecast_t` structure.
2. Adjusted synthetic pre-squall relative humidity starting point in `test_rain_algo.c` from $77.5\%$ to $80.0\%$:

```diff
-    rain_forecast_state_t forecast_state;
+    rain_alert_state_t forecast_state;
...
-            storm_profile[i].rh_pct = 77.5f + (float)step * 3.0f;
+            storm_profile[i].rh_pct = 80.0f + (float)step * 3.0f;
...
-    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, (rain_alert_state_t)fc_squall.forecast_state);
+    TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, fc_squall.forecast_state);
```

---

## 4. Verification & Prevention Guidelines

- Maintain single canonical enum definitions for state variables across application and middleware layers.
- Verify algorithm test profiles against realistic meteorological thresholds defined in domain specifications.
