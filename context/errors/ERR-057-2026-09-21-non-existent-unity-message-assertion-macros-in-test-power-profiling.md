# Error Report: ERR-057 - Non-Existent TEST_ASSERT_TRUE_MESSAGE & TEST_ASSERT_FLOAT_WITHIN_MESSAGE in test_power_profiling.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-057` |
| **Date & Time** | 2026-09-21 08:05:09 +05:30 |
| **Commit SHA** | [`c77044d`](https://github.com/Thejus26/rain-predict/commit/c77044da742343fe4c3a3c273f8405e46cb1925e) |
| **Component / Subsystem** | Testing Macros & Mock Architectural Collisions |
| **Sprint & Task** | Sprint 7 (`S7-T2.1` Stop 2 Deep Sleep & Active Cycle Power Profiling) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=implicit-function-declaration`) |
| **Impacted Files** | [`tests/integration/test_power_profiling.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_power_profiling.c) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI host test compilation (`/usr/bin/cc` with `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage`), compilation of [`tests/integration/test_power_profiling.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_power_profiling.c) failed with fatal implicit function declaration errors:

```text
FAILED: [code=1] tests/CMakeFiles/test_power_profiling.dir/integration/test_power_profiling.c.o 
/usr/bin/cc -DHOST_TEST=1 -DUNITY_INCLUDE_CONFIG_H=1 -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_power_profiling.dir/integration/test_power_profiling.c.o -MF tests/CMakeFiles/test_power_profiling.dir/integration/test_power_profiling.c.o.d -o tests/CMakeFiles/test_power_profiling.dir/integration/test_power_profiling.c.o -c /home/runner/work/rain-predict/rain-predict/tests/integration/test_power_profiling.c
/home/runner/work/rain-predict/rain-predict/tests/integration/test_power_profiling.c: In function ‘test_TC_PWR_01_stop2_deep_sleep_current_room_temp’:
/home/runner/work/rain-predict/rain-predict/tests/integration/test_power_profiling.c:160:5: error: implicit declaration of function ‘TEST_ASSERT_TRUE_MESSAGE’; did you mean ‘TEST_ASSERT_TRUE’? [-Werror=implicit-function-declaration]
  160 |     TEST_ASSERT_TRUE_MESSAGE(stop2_current_ua < STOP2_SLEEP_CURRENT_MAX_UA,
      |     ^~~~~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_TRUE
/home/runner/work/rain-predict/rain-predict/tests/integration/test_power_profiling.c:162:5: error: implicit declaration of function ‘TEST_ASSERT_FLOAT_WITHIN_MESSAGE’; did you mean ‘TEST_ASSERT_FLOAT_WITHIN’? [-Werror=implicit-function-declaration]
  162 |     TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, STOP2_SLEEP_CURRENT_NOM_UA, stop2_current_ua,
      |     ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_FLOAT_WITHIN
```

---

## 2. Root Cause Analysis

In [`tests/integration/test_power_profiling.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_power_profiling.c), all 12 power verification unit test functions (`test_TC_PWR_01_...` through `test_TC_PWR_12_...`) made calls to `TEST_ASSERT_TRUE_MESSAGE(...)` and `TEST_ASSERT_FLOAT_WITHIN_MESSAGE(...)`.

However, the repository's Unity test framework header [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h) defines:
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`
- `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)`
- `TEST_ASSERT_EQUAL_FLOAT(expected, actual)`
- `TEST_FAIL_MESSAGE(message)`
- `TEST_IGNORE_MESSAGE(message)`

It does **not** define `_MESSAGE` variants for boolean or floating-point assertions (unlike full upstream Unity distributions or custom message extensions). In C99 under strict GCC diagnostic flags (`-Wall -Wextra -Werror`), invoking an undeclared macro name causes the compiler to assume an implicit function declaration returning `int`, which immediately triggers `-Werror=implicit-function-declaration` and halts compilation.

---

## 3. Resolution & Code Changes *(Implemented)*

### Fix Strategy
1. Replaced all occurrences of `TEST_ASSERT_TRUE_MESSAGE(condition, message)` with canonical `TEST_ASSERT_TRUE(condition)` across all 12 test functions in [`tests/integration/test_power_profiling.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/integration/test_power_profiling.c).
2. Replaced all occurrences of `TEST_ASSERT_FLOAT_WITHIN_MESSAGE(delta, expected, actual, message)` with canonical `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)`.

### Code Changes
```diff
--- a/tests/integration/test_power_profiling.c
+++ b/tests/integration/test_power_profiling.c
@@ -157,10 +157,8 @@ void tearDown(void) {
  */
 static void test_TC_PWR_01_stop2_deep_sleep_current_room_temp(void) {
     float stop2_current_ua = 3.0f;
-    TEST_ASSERT_TRUE_MESSAGE(stop2_current_ua < STOP2_SLEEP_CURRENT_MAX_UA,
-                             "Stop 2 standby current exceeds 5.0 uA ceiling at 25°C");
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, STOP2_SLEEP_CURRENT_NOM_UA, stop2_current_ua,
-                                    "Nominal Stop 2 baseline deviates from expected 3.0 uA");
+    TEST_ASSERT_TRUE(stop2_current_ua < STOP2_SLEEP_CURRENT_MAX_UA);
+    TEST_ASSERT_FLOAT_WITHIN(0.5f, STOP2_SLEEP_CURRENT_NOM_UA, stop2_current_ua);
 }
 
 /**
@@ -168,36 +166,30 @@ static void test_TC_PWR_01_stop2_deep_sleep_current_room_temp(void) {
  */
 static void test_TC_PWR_02_stop2_elevated_temperature_current(void) {
     float stop2_elevated_ua = 6.5f;
-    TEST_ASSERT_TRUE_MESSAGE(stop2_elevated_ua < STOP2_SLEEP_ELEVATED_MAX_UA,
-                             "Stop 2 standby current exceeds 8.0 uA ceiling at 50°C tropical profile");
+    TEST_ASSERT_TRUE(stop2_elevated_ua < STOP2_SLEEP_ELEVATED_MAX_UA);
 }
 
 /**
  * @brief TC-PWR-03: Total Active Cycle Duration (<= 1200.0 ms / 1.20s).
  */
 static void test_TC_PWR_03_total_active_cycle_duration_ceiling(void) {
-    TEST_ASSERT_TRUE_MESSAGE(s_nominal_summary.total_active_ms <= ACTIVE_CYCLE_MAX_MS,
-                             "Active execution duration exceeds 1.20s maximum CPU window");
+    TEST_ASSERT_TRUE(s_nominal_summary.total_active_ms <= ACTIVE_CYCLE_MAX_MS);
 }
 
 /**
  * @brief TC-PWR-04: Nominal Active Duration Floor (<= 250.0 ms, target ~166.5 ms).
  */
 static void test_TC_PWR_04_nominal_active_duration_floor(void) {
-    TEST_ASSERT_TRUE_MESSAGE(s_nominal_summary.total_active_ms <= ACTIVE_NOMINAL_FLOOR_MAX_MS,
-                             "Nominal single-shot execution duration exceeds 250 ms target floor");
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(2.0f, 166.5f, s_nominal_summary.total_active_ms,
-                                    "Nominal state sequence timing deviates from 166.5 ms profile");
+    TEST_ASSERT_TRUE(s_nominal_summary.total_active_ms <= ACTIVE_NOMINAL_FLOOR_MAX_MS);
+    TEST_ASSERT_FLOAT_WITHIN(2.0f, 166.5f, s_nominal_summary.total_active_ms);
 }
 
 /**
  * @brief TC-PWR-05: Active Cycle Average Operating Current (< 25.0 mA, target ~15.1 mA).
  */
 static void test_TC_PWR_05_active_cycle_average_current(void) {
-    TEST_ASSERT_TRUE_MESSAGE(s_nominal_summary.active_avg_current_ma < ACTIVE_AVG_CURRENT_MAX_MA,
-                             "Average active cycle current exceeds 25.0 mA ceiling");
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(1.0f, 15.14f, s_nominal_summary.active_avg_current_ma,
-                                    "Average active current deviates from 15.14 mA model");
+    TEST_ASSERT_TRUE(s_nominal_summary.active_avg_current_ma < ACTIVE_AVG_CURRENT_MAX_MA);
+    TEST_ASSERT_FLOAT_WITHIN(1.0f, 15.14f, s_nominal_summary.active_avg_current_ma);
 }
 
 /**
@@ -205,8 +197,7 @@ static void test_TC_PWR_05_active_cycle_average_current(void) {
  */
 static void test_TC_PWR_06_sensor_rail_stabilization_guard(void) {
     float power_on_duration_ms = s_state_specs[1].duration_ms;
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(SENSOR_STABILIZE_TOL_MS, SENSOR_STABILIZE_TARGET_MS, power_on_duration_ms,
-                                    "STATE_POWER_ON duration violates mandatory 20.0 ms RC stabilization delay");
+    TEST_ASSERT_FLOAT_WITHIN(SENSOR_STABILIZE_TOL_MS, SENSOR_STABILIZE_TARGET_MS, power_on_duration_ms);
 }
 
 /**
@@ -214,8 +205,7 @@ static void test_TC_PWR_06_sensor_rail_stabilization_guard(void) {
  */
 static void test_TC_PWR_07_pre_sleep_analog_gpio_isolation_leakage(void) {
     float measured_gpio_leakage_na = 35.0f;
-    TEST_ASSERT_TRUE_MESSAGE(measured_gpio_leakage_na < GPIO_LEAKAGE_MAX_NA,
-                             "Parasitic clamping diode leakage through unpowered sensor rail exceeds 50 nA");
+    TEST_ASSERT_TRUE(measured_gpio_leakage_na < GPIO_LEAKAGE_MAX_NA);
 }
 
 /**
@@ -223,8 +213,7 @@ static void test_TC_PWR_07_pre_sleep_analog_gpio_isolation_leakage(void) {
  */
 static void test_TC_PWR_08_gated_battery_adc_divider_leakage(void) {
     float measured_divider_leakage_na = 5.0f;
-    TEST_ASSERT_TRUE_MESSAGE(measured_divider_leakage_na < DIVIDER_LEAKAGE_MAX_NA,
-                             "Standby leakage current through gated battery divider exceeds 10 nA");
+    TEST_ASSERT_TRUE(measured_divider_leakage_na < DIVIDER_LEAKAGE_MAX_NA);
 }
 
 /**
@@ -236,12 +225,9 @@ static void test_TC_PWR_09_lorawan_rf_tx_energy_standard(void) {
     float tx_peak_ma = s_state_specs[5].peak_current_ma;
     float tx_energy_mj = calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, tx_curr_ma, tx_dur_ms);
 
-    TEST_ASSERT_TRUE_MESSAGE(tx_energy_mj <= LORA_TX_ENERGY_MAX_MJ,
-                             "+14 dBm LoRa RF transmission energy exceeds 7.5 mJ ceiling");
-    TEST_ASSERT_TRUE_MESSAGE(tx_peak_ma <= LORA_TX_PEAK_CURRENT_MAX_MA,
-                             "+14 dBm LoRa RF peak current exceeds 36.0 mA ceiling");
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 6.336f, tx_energy_mj,
-                                    "LoRa TX energy deviates from expected 6.336 mJ");
+    TEST_ASSERT_TRUE(tx_energy_mj <= LORA_TX_ENERGY_MAX_MJ);
+    TEST_ASSERT_TRUE(tx_peak_ma <= LORA_TX_PEAK_CURRENT_MAX_MA);
+    TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.336f, tx_energy_mj);
 }
 
 /**
@@ -252,10 +238,8 @@ static void test_TC_PWR_10_high_power_rf_tx_headroom(void) {
     float tx_high_dur_ms = 80.0f;
     float tx_high_energy_mj = calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, tx_high_peak_ma, tx_high_dur_ms);
 
-    TEST_ASSERT_TRUE_MESSAGE(tx_high_peak_ma <= LORA_TX_HIGH_POWER_PEAK_MAX_MA,
-                             "+22 dBm fallback peak current exceeds 90.0 mA maximum headroom");
-    TEST_ASSERT_TRUE_MESSAGE(tx_high_energy_mj <= LORA_TX_HIGH_POWER_MAX_MJ,
-                             "+22 dBm fallback energy exceeds 25.0 mJ ceiling");
+    TEST_ASSERT_TRUE(tx_high_peak_ma <= LORA_TX_HIGH_POWER_PEAK_MAX_MA);
+    TEST_ASSERT_TRUE(tx_high_energy_mj <= LORA_TX_HIGH_POWER_MAX_MJ);
 }
 
 /**
@@ -266,22 +250,17 @@ static void test_TC_PWR_11_flash_nvm_programming_energy(void) {
     float nvm_dur_ms = 5.0f;
     float nvm_energy_mj = calc_energy_mj(NOMINAL_SYSTEM_VOLTAGE_V, nvm_curr_ma, nvm_dur_ms);
 
-    TEST_ASSERT_TRUE_MESSAGE(nvm_energy_mj <= FLASH_NVM_ENERGY_MAX_MJ,
-                             "Flash double-word write & circular pointer update exceeds 0.10 mJ");
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, 0.0825f, nvm_energy_mj,
-                                    "Flash NVM energy deviates from 0.0825 mJ");
+    TEST_ASSERT_TRUE(nvm_energy_mj <= FLASH_NVM_ENERGY_MAX_MJ);
+    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0825f, nvm_energy_mj);
 }
 
 /**
  * @brief TC-PWR-12: Total Single-Cycle Charge for 15-min Mission (<= 1.60 uAh / 0.0016 mAh).
  */
 static void test_TC_PWR_12_total_single_cycle_charge_15min(void) {
-    TEST_ASSERT_TRUE_MESSAGE(s_nominal_summary.single_cycle_charge_uah <= CYCLE_TOTAL_CHARGE_MAX_UAH,
-                             "Total 15-minute mission cycle charge exceeds 1.60 uAh ceiling");
-    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.05f, 1.450f, s_nominal_summary.single_cycle_charge_uah,
-                                    "Total single-cycle charge deviates from 1.450 uAh target");
-    TEST_ASSERT_TRUE_MESSAGE(s_nominal_summary.mission_avg_current_ua < 6.0f,
-                             "Average 15-minute mission current exceeds 6.0 uA ceiling");
+    TEST_ASSERT_TRUE(s_nominal_summary.single_cycle_charge_uah <= CYCLE_TOTAL_CHARGE_MAX_UAH);
+    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.450f, s_nominal_summary.single_cycle_charge_uah);
+    TEST_ASSERT_TRUE(s_nominal_summary.mission_avg_current_ua < 6.0f);
 }
 ```

---

## 4. Verification & Prevention Guidelines

1. **Adherence to Whitelisted Unity Assertion Macros**:
   - Refer to Section 10.4 of [`context/coding-standards.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/coding-standards.md) before introducing new test assertions. Standard Unity headers in this repository only support base assertion macros (`TEST_ASSERT_TRUE`, `TEST_ASSERT_FALSE`, `TEST_ASSERT_FLOAT_WITHIN`, `TEST_ASSERT_EQUAL_*`).
   - Do not assume `_MESSAGE` variants are available unless explicitly defined in [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h).
2. **Prior Incidents Parity**:
   - Matches incident [`ERR-053`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/ERR-053-2026-09-20-non-existent-test-assert-true-message-in-test-simulation-validation.md) where `TEST_ASSERT_TRUE_MESSAGE` was previously identified as undefined.
