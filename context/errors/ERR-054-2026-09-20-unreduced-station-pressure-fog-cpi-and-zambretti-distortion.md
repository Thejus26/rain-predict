# Error Report: ERR-054 - Unreduced Station Pressure in test_simulation_validation.c Causes False Fog CPI Surge and Zambretti Distortion

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-054` |
| **Date & Time** | 2026-09-20 21:03:00 +05:30 |
| **Commit SHA** | [`159fbc1`](https://github.com/Thejus26/rain-predict/commit/159fbc1e5da6237caba9940a7f7baa7196ddaa79) |
| **Component / Subsystem** | Algorithm Numerical Precision & Physical Vectors |
| **Sprint & Task** | Sprint 7 (`S7-T1.1` 30-Day Multi-Scenario Synthetic Climate Simulation & Firmware Algorithm Validation) |
| **Severity** | High (Integration Test Failure in Remote GitHub Actions CI) |
| **Impacted Files** | [`tests/integration/test_simulation_validation.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_simulation_validation.c) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI execution of CTest `SimulationValidationTest`, two unit assertions failed in `test_simulation_validation.c`:

```text
/home/runner/work/rain-predict/rain-predict/tests/integration/test_simulation_validation.c:471:test_sim_08_morning_valley_fog_rejection:FAIL: Expected 1 Was 0.  Expected TRUE Was FALSE
/home/runner/work/rain-predict/rain-predict/tests/integration/test_simulation_validation.c:550:test_sim_09_passing_cloud_shadow_rejection:PASS
/home/runner/work/rain-predict/rain-predict/tests/integration/test_simulation_validation.c:498:test_sim_10_fair_weather_quiescent_stability:FAIL: Expected 1 Was 0.  Expected TRUE Was FALSE
```

- Line 471 (`test_sim_08_morning_valley_fog_rejection`): `TEST_ASSERT_TRUE(s_forecasts[i].cpi_score_pct < 40.0f);` failed because evaluated `cpi_score_pct` reached 46.0%.
- Line 498 (`test_sim_10_fair_weather_quiescent_stability`): `TEST_ASSERT_TRUE(s_forecasts[i].z_index <= 4U);` failed because evaluated `z_index` reached 9–10 during high-pressure fair-weather ridge days (Days 23–27).

---

## 2. Root Cause Analysis

In [`tests/integration/test_simulation_validation.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_simulation_validation.c), the synthetic climate generator `sim_generate_30day_dataset()` creates station-level barometric pressure `s_climate_data[step].pressure_hpa` (~840–860 hPa) reduced for $1,500\text{ m}$ AMSL using the hypsometric formula.

However, in `sim_run_firmware_nowcasting()`:
```c
env_sample_t new_sample;
new_sample.temp_c      = s_climate_data[i].temp_c;
new_sample.rh_pct      = s_climate_data[i].humidity_pct;
new_sample.p0_hpa      = s_climate_data[i].pressure_hpa;  /* <--- Raw station pressure (~840 hPa) assigned to p0_hpa */
new_sample.lux         = s_climate_data[i].solar_lux;
new_sample.timestamp_s = i * 900U;
```
`env_sample_t.p0_hpa` is documented in `trend_detector.h` as **sea-level reduced pressure ($P_0 \approx 1013\text{ hPa}$)**.
When station pressure (~840 hPa) was stored in `new_sample.p0_hpa`:
1. `trend_detector_compute_gradients()` computed barometric gradients ($\Delta P_{1\text{h}}$ and $\Delta P_{3\text{h}}$) directly on the unreduced mountain station pressure. At $1,500\text{ m}$ elevation, diurnal temperature variations ($15^\circ\text{C}$ to $27.5^\circ\text{C}$) alter air density, inducing an artificial diurnal pressure swing of up to $\pm 2.7\text{ hPa}$ in station pressure even when true synoptic sea-level pressure $P_0$ is rising or constant.
2. During high-pressure ridge evenings (Days 23–27), nocturnal cooling caused station pressure to drop by $-1.6\text{ to } -2.7\text{ hPa}$ over 3 hours, tricking Zambretti into classifying the trend as `FALLING` and computing $Z \in [9, 10]$ instead of $Z \in [1, 4]$ (`SETTLED_FINE`).
3. During morning valley radiation fog (Days 16, 17, 19), cooling air generated a negative pressure gradient $\Delta P_{1\text{h}} < 0$, yielding `score_pressure = 50`. Coupled with fog humidity ($88\%$, `score_humidity = 80`) and low dew point depression (`score_dew_point = 80`), the nighttime re-normalized CPI rose to $46.0\%$ ($\ge 40\%$), failing the fog rejection threshold.

---

## 3. Resolution & Code Changes

### Fix Strategy
1. In `sim_run_firmware_nowcasting()`, reduce raw station pressure `s_climate_data[i].pressure_hpa` to sea-level equivalent pressure $P_0$ using `dew_point_calc_sea_level_pressure()` before populating `new_sample.p0_hpa`.
2. When invoking `rain_algo_evaluate()`, pass `altitude_m = 0.0f` since `history` samples already contain the standardized sea-level pressure $P_0$.

### Code Changes *(Implemented)*
```diff
--- a/tests/integration/test_simulation_validation.c
+++ b/tests/integration/test_simulation_validation.c
@@ -316,10 +316,19 @@ static void sim_run_firmware_nowcasting(void) {
     uint32_t history_count = 0U;
 
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
+        float p0_sea = 0.0f;
+        status_t rc_p0 = dew_point_calc_sea_level_pressure(s_climate_data[i].pressure_hpa,
+                                                            s_climate_data[i].temp_c,
+                                                            SIM_ELEVATION_M,
+                                                            &p0_sea);
+        if (rc_p0 != STATUS_OK) {
+            p0_sea = s_climate_data[i].pressure_hpa;
+        }
+
         env_sample_t new_sample;
         new_sample.temp_c      = s_climate_data[i].temp_c;
         new_sample.rh_pct      = s_climate_data[i].humidity_pct;
-        new_sample.p0_hpa      = s_climate_data[i].pressure_hpa;
+        new_sample.p0_hpa      = p0_sea;
         new_sample.lux         = s_climate_data[i].solar_lux;
         new_sample.timestamp_s = i * 900U;
 
@@ -336,7 +345,7 @@ static void sim_run_firmware_nowcasting(void) {
         (void)trend_detector_compute_gradients(history, history_count, &s_gradients[i]);
         (void)rain_algo_evaluate(history,
                                  history_count,
-                                 SIM_ELEVATION_M,
+                                 0.0f,
                                  6U,
                                  WIND_DIR_CALM,
                                  0.0f,
```

---

## 4. Verification & Prevention Guidelines

1. **Hypsometric Domain Integrity**: In embedded sensor pipelines, ensure station pressure is reduced to $P_0$ at the ingestion/filtering stage (`STATE_FILTER`) before appending to gradient tracking history buffers (`p_samples->p0_hpa`), ensuring barometric deltas represent synoptic weather trends rather than diurnal thermal density shifts.
2. **Simulation Integration Harness Alignment**: Test suites that simulate mountain microclimates must mirror the actual firmware lifecycle where raw transducer readings are normalized to $P_0$ before computing rolling window differentials.
