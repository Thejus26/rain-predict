# Error Report: ERR-052 - Missing static Function Prototypes in test_simulation_validation.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-052` |
| **Date & Time** | 2026-09-20 20:35:00 +05:30 |
| **Commit SHA** | [`62983d2`](https://github.com/Thejus26/rain-predict/commit/62983d215e4b2a0314e6617fe7173a626cec5319) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Wmissing-prototypes, -Werror) |
| **Sprint & Task** | Sprint 7 (`S7-T1.1` 30-Day Multi-Scenario Synthetic Climate Simulation & Firmware Algorithm Validation) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=missing-prototypes`) |
| **Impacted Files** | [`tests/integration/test_simulation_validation.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_simulation_validation.c) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI host test compilation (`/usr/bin/cc` with `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage`), compilation of `tests/integration/test_simulation_validation.c` failed with `-Wmissing-prototypes` errors:

```text
FAILED: [code=1] tests/CMakeFiles/test_simulation_validation.dir/integration/test_simulation_validation.c.o 
/usr/bin/cc -DHOST_TEST=1 -DUNITY_INCLUDE_CONFIG_H=1 -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_simulation_validation.dir/integration/test_simulation_validation.c.o -MF tests/CMakeFiles/test_simulation_validation.dir/integration/test_simulation_validation.c.o.d -o tests/CMakeFiles/test_simulation_validation.dir/integration/test_simulation_validation.c.o -c /home/runner/work/rain-predict/rain-predict/tests/integration/test_simulation_validation.c
/home/runner/work/rain-predict/rain-predict/tests/integration/test_simulation_validation.c:386:6: error: no previous prototype for ‘test_sim_01_continuous_30day_stepping’ [-Werror=missing-prototypes]
  386 | void test_sim_01_continuous_30day_stepping(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cc1: all warnings being treated as errors
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

In [`tests/integration/test_simulation_validation.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_simulation_validation.c), all 12 Unity test cases were defined with external linkage (`void test_sim_...`) without forward prototype declarations:
- `test_sim_01_continuous_30day_stepping`
- `test_sim_02_physical_boundary_invariants`
- `test_sim_03_dew_point_mathematical_bounds`
- `test_sim_04_convective_cloudburst_detection`
- `test_sim_05_precursor_optical_attenuation`
- `test_sim_06_severe_barometric_override`
- `test_sim_07_sustained_monsoon_tracking`
- `test_sim_08_morning_valley_fog_rejection`
- `test_sim_09_passing_cloud_shadow_rejection`
- `test_sim_10_fair_weather_quiescent_stability`
- `test_sim_11_deterministic_seed_repeatability`
- `test_sim_12_host_execution_performance`

The project's strict host build configuration (`CompilerFlags.cmake`) enforces `-Wmissing-prototypes -Werror`. In ISO C99, any global function definition with external linkage must be preceded by a prototype declaration to ensure prototype consistency across translation units. Because these Unity test functions are only called from `main()` within the same translation unit via `RUN_TEST()`, they should have internal linkage (`static void`).

---

## 3. Resolution & Code Changes

### Fix Strategy
Mark all 12 test functions in [`tests/integration/test_simulation_validation.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_simulation_validation.c) with the `static` qualifier:
`static void test_sim_XX_...(void)`.

### Code Changes *(Implemented)*
```diff
--- a/tests/integration/test_simulation_validation.c
+++ b/tests/integration/test_simulation_validation.c
@@ -383,7 +383,7 @@ void tearDown(void) {
 /* 12 Simulation Invariant Test Cases (TC-SIM-01 .. TC-SIM-12)               */
 /* ========================================================================== */
 
-void test_sim_01_continuous_30day_stepping(void) {
+static void test_sim_01_continuous_30day_stepping(void) {
     TEST_ASSERT_EQUAL_UINT32(2880U, SIM_TOTAL_STEPS);
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         TEST_ASSERT_FALSE(isnan(s_climate_data[i].temp_c));
@@ -394,7 +394,7 @@ void test_sim_01_continuous_30day_stepping(void) {
     }
 }
 
-void test_sim_02_physical_boundary_invariants(void) {
+static void test_sim_02_physical_boundary_invariants(void) {
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         TEST_ASSERT_TRUE(s_climate_data[i].temp_c >= 10.0f && s_climate_data[i].temp_c <= 36.0f);
         TEST_ASSERT_TRUE(s_climate_data[i].humidity_pct >= 35.0f && s_climate_data[i].humidity_pct <= 100.0f);
@@ -403,7 +403,7 @@ void test_sim_02_physical_boundary_invariants(void) {
     }
 }
 
-void test_sim_03_dew_point_mathematical_bounds(void) {
+static void test_sim_03_dew_point_mathematical_bounds(void) {
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         float tdew = 0.0f;
         float dpd  = 0.0f;
@@ -414,7 +414,7 @@ void test_sim_03_dew_point_mathematical_bounds(void) {
     }
 }
 
-void test_sim_04_convective_cloudburst_detection(void) {
+static void test_sim_04_convective_cloudburst_detection(void) {
     uint32_t phase1_storm_count = 0U;
     for (uint32_t e = 0; e < s_rain_event_count; e++) {
         if (s_rain_events[e].phase_id == 1U) {
@@ -426,7 +426,7 @@ void test_sim_04_convective_cloudburst_detection(void) {
     TEST_ASSERT_EQUAL_UINT32(4U, phase1_storm_count);
 }
 
-void test_sim_05_precursor_optical_attenuation(void) {
+static void test_sim_05_precursor_optical_attenuation(void) {
     uint32_t blackout_drop_count = 0U;
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         float hour = fmodf(s_climate_data[i].timestamp_hour, 24.0f);
@@ -440,7 +440,7 @@ void test_sim_05_precursor_optical_attenuation(void) {
     TEST_ASSERT_TRUE(blackout_drop_count > 0U);
 }
 
-void test_sim_06_severe_barometric_override(void) {
+static void test_sim_06_severe_barometric_override(void) {
     uint32_t override_count = 0U;
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         if (s_gradients[i].delta_p_1h_hpa <= CRITICAL_DROP_1H_HPA &&
@@ -452,7 +452,7 @@ void test_sim_06_severe_barometric_override(void) {
     TEST_ASSERT_TRUE(override_count > 0U);
 }
 
-void test_sim_07_sustained_monsoon_tracking(void) {
+static void test_sim_07_sustained_monsoon_tracking(void) {
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         if (s_climate_data[i].phase_id == 2U) {
             /* Must never false-clear down to UNLIKELY during sustained monsoon */
@@ -461,7 +461,7 @@ void test_sim_07_sustained_monsoon_tracking(void) {
     }
 }
 
-void test_sim_08_morning_valley_fog_rejection(void) {
+static void test_sim_08_morning_valley_fog_rejection(void) {
     uint32_t fog_step_count = 0U;
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         uint32_t day = s_climate_data[i].day_index;
@@ -475,7 +475,7 @@ void test_sim_08_morning_valley_fog_rejection(void) {
     TEST_ASSERT_TRUE(fog_step_count > 0U);
 }
 
-void test_sim_09_passing_cloud_shadow_rejection(void) {
+static void test_sim_09_passing_cloud_shadow_rejection(void) {
     uint32_t shadow_step_count = 0U;
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         uint32_t day = s_climate_data[i].day_index;
@@ -489,7 +489,7 @@ void test_sim_09_passing_cloud_shadow_rejection(void) {
     TEST_ASSERT_TRUE(shadow_step_count > 0U);
 }
 
-void test_sim_10_fair_weather_quiescent_stability(void) {
+static void test_sim_10_fair_weather_quiescent_stability(void) {
     uint32_t ridge_step_count = 0U;
     for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
         if (s_climate_data[i].day_index >= 23U && s_climate_data[i].day_index <= 27U) {
@@ -502,7 +502,7 @@ void test_sim_10_fair_weather_quiescent_stability(void) {
     TEST_ASSERT_EQUAL_UINT32(480U, ridge_step_count);
 }
 
-void test_sim_11_deterministic_seed_repeatability(void) {
+static void test_sim_11_deterministic_seed_repeatability(void) {
     /* Store first cycle snapshot */
     float first_cpi_day2_storm = s_forecasts[150].cpi_score_pct;
 
@@ -513,7 +513,7 @@ void test_sim_11_deterministic_seed_repeatability(void) {
     TEST_ASSERT_FLOAT_WITHIN(0.001f, first_cpi_day2_storm, s_forecasts[150].cpi_score_pct);
 }
 
-void test_sim_12_host_execution_performance(void) {
+static void test_sim_12_host_execution_performance(void) {
     clock_t start = clock();
 
     /* Benchmark full 2,880-step generation, gradient math, and algorithm evaluation */
```

---

## 4. Verification & Prevention Guidelines

1. **Unity Test Linkage Rule**: All test functions in test translation units (`test_*.c`) that are called exclusively via `RUN_TEST()` in `main()` must be declared `static void` to satisfy `-Wmissing-prototypes` without cluttering header files.
2. **Compiler Flags Alignment**: Strict C99 warnings (`-Wmissing-prototypes -Wstrict-prototypes -Werror`) apply to all integration and unit test targets.
3. **Pre-Completion Checklist**: Verify that all new C test functions added in future sprint tasks are marked `static` before finalizing the feature.
