# Error Report: ERR-041 - Missing static Function Prototypes in test_alert_manager.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-041` |
| **Date & Time** | 2026-09-19 22:06:00 +05:30 |
| **Commit SHA** | [`982698c`](https://github.com/Thejus26/rain-predict/commit/982698c22b24fd5ad8245218c74d63ac9f1e1e2e) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Wmissing-prototypes, -Werror) |
| **Sprint & Task** | Sprint 6 (`S6-T2.3` Alert Manager Unit Test Suite) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=missing-prototypes`) |
| **Impacted Files** | [`tests/unit/test_alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_alert_manager.c)<br>[`firmware/app/inc/alert_manager.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/alert_manager.h)<br>[`firmware/app/src/alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c) |

---

## 1. Description & Symptoms

During GitHub Actions CI host test compilation (`/usr/bin/cc` with `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror`), compilation of `test_alert_manager.c` failed with `-Wmissing-prototypes` errors:

```text
FAILED: [code=1] tests/CMakeFiles/test_alert_manager.dir/unit/test_alert_manager.c.o 
/usr/bin/cc -DUNITY_INCLUDE_CONFIG_H -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/tests/mocks -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_alert_manager.dir/unit/test_alert_manager.c.o -MF tests/CMakeFiles/test_alert_manager.dir/unit/test_alert_manager.c.o.d -o tests/CMakeFiles/test_alert_manager.dir/unit/test_alert_manager.c.o -c /home/runner/work/rain-predict/rain-predict/tests/unit/test_alert_manager.c
/home/runner/work/rain-predict/rain-predict/tests/unit/test_alert_manager.c:46:6: error: no previous prototype for ‘test_alert_init_defaults’ [-Werror=missing-prototypes]
   46 | void test_alert_init_defaults(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~
cc1: all warnings being treated as errors
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_alert_manager.c), unit test functions across Categories A through D were declared with global external linkage (`void test_...`) without forward prototype declarations:
- `test_alert_init_defaults`
- `test_alert_init_custom_config`
- `test_alert_force_all_off`
- `test_alert_manual_led_override`
- `test_alert_manual_buzzer_override`
- `test_alert_led_healthy_pulse_timing`
- `test_alert_led_watch_amber_timing`
- `test_alert_led_warning_red_timing`
- `test_alert_led_imminent_strobe_timing`
- `test_alert_led_active_rain_double_flash`
- `test_alert_led_system_fault_beacon`
- `test_alert_buzzer_watch_single_chirp`
- `test_alert_buzzer_warning_double_chirp`
- `test_alert_storm_siren_auto_trigger`
- `test_alert_siren_10s_auto_cutoff`
- `test_alert_siren_30min_cooldown`
- `test_alert_night_quiet_hours_mute`
- `test_alert_battery_conservation_tier2`
- `test_alert_battery_critical_tier3`
- `test_alert_sensor_fault_preemption`
- `test_alert_active_rain_preemption`
- `test_alert_full_storm_lifecycle_sim`
- `test_alert_defensive_null_and_guards`

Under the project's strict compiler flag configuration (`-std=c99 -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror`), GCC enforces that every function with external linkage must be preceded by a prototype declaration in an included header or file scope. While `setUp(void)` and `tearDown(void)` have prototypes declared in `unity.h`, test functions defined in test translation units are only called locally via `RUN_TEST(...)` in `main()` and should possess internal linkage with `static void`.

---

## 3. Resolution & Code Changes

### Resolution Steps

1. Added `static` storage-class specifiers to all 24 unit test function definitions in [`tests/unit/test_alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_alert_manager.c) across Categories A through D:
   - `static void test_alert_init_defaults(void)`
   - `static void test_alert_init_custom_config(void)`
   - `static void test_alert_force_all_off(void)`
   - `static void test_alert_manual_led_override(void)`
   - `static void test_alert_manual_buzzer_override(void)`
   - `static void test_alert_pattern_name_lookup(void)`
   - `static void test_alert_led_healthy_pulse_timing(void)`
   - `static void test_alert_led_watch_amber_timing(void)`
   - `static void test_alert_led_warning_red_timing(void)`
   - `static void test_alert_led_imminent_strobe_timing(void)`
   - `static void test_alert_led_active_rain_double_flash(void)`
   - `static void test_alert_led_system_fault_beacon(void)`
   - `static void test_alert_buzzer_watch_single_chirp(void)`
   - `static void test_alert_buzzer_warning_double_chirp(void)`
   - `static void test_alert_storm_siren_auto_trigger(void)`
   - `static void test_alert_siren_10s_auto_cutoff(void)`
   - `static void test_alert_siren_30min_cooldown(void)`
   - `static void test_alert_night_quiet_hours_mute(void)`
   - `static void test_alert_battery_conservation_tier2(void)`
   - `static void test_alert_battery_critical_tier3(void)`
   - `static void test_alert_sensor_fault_preemption(void)`
   - `static void test_alert_active_rain_preemption(void)`
   - `static void test_alert_full_storm_lifecycle_sim(void)`
   - `static void test_alert_defensive_null_and_guards(void)`
2. Retained external linkage for Unity lifecycle hooks `setUp(void)` and `tearDown(void)` (prototyped by `unity.h`) and `main(void)`.
3. Verified zero `-Wmissing-prototypes` compiler warnings.

### Code Diff

```diff
diff --git a/tests/unit/test_alert_manager.c b/tests/unit/test_alert_manager.c
index 4fee839..9f1dce5 100644
--- a/tests/unit/test_alert_manager.c
+++ b/tests/unit/test_alert_manager.c
@@ -46,3 +46,3 @@
-void test_alert_init_defaults(void) {
+static void test_alert_init_defaults(void) {
@@ -63,3 +63,3 @@
-void test_alert_init_custom_config(void) {
+static void test_alert_init_custom_config(void) {
@@ -84,3 +84,3 @@
-void test_alert_force_all_off(void) {
+static void test_alert_force_all_off(void) {
@@ -97,3 +97,3 @@
-void test_alert_manual_led_override(void) {
+static void test_alert_manual_led_override(void) {
@@ -108,3 +108,3 @@
-void test_alert_manual_buzzer_override(void) {
+static void test_alert_manual_buzzer_override(void) {
@@ -118,3 +118,3 @@
-void test_alert_pattern_name_lookup(void) {
+static void test_alert_pattern_name_lookup(void) {
@@ -131,3 +131,3 @@
-void test_alert_led_healthy_pulse_timing(void) {
+static void test_alert_led_healthy_pulse_timing(void) {
@@ -158,3 +158,3 @@
-void test_alert_led_watch_amber_timing(void) {
+static void test_alert_led_watch_amber_timing(void) {
@@ -188,3 +188,3 @@
-void test_alert_led_warning_red_timing(void) {
+static void test_alert_led_warning_red_timing(void) {
@@ -212,3 +212,3 @@
-void test_alert_led_imminent_strobe_timing(void) {
+static void test_alert_led_imminent_strobe_timing(void) {
@@ -234,3 +234,3 @@
-void test_alert_led_active_rain_double_flash(void) {
+static void test_alert_led_active_rain_double_flash(void) {
@@ -261,3 +261,3 @@
-void test_alert_led_system_fault_beacon(void) {
+static void test_alert_led_system_fault_beacon(void) {
@@ -289,3 +289,3 @@
-void test_alert_buzzer_watch_single_chirp(void) {
+static void test_alert_buzzer_watch_single_chirp(void) {
@@ -307,3 +307,3 @@
-void test_alert_buzzer_warning_double_chirp(void) {
+static void test_alert_buzzer_warning_double_chirp(void) {
@@ -333,3 +333,3 @@
-void test_alert_storm_siren_auto_trigger(void) {
+static void test_alert_storm_siren_auto_trigger(void) {
@@ -348,3 +348,3 @@
-void test_alert_siren_10s_auto_cutoff(void) {
+static void test_alert_siren_10s_auto_cutoff(void) {
@@ -364,3 +364,3 @@
-void test_alert_siren_30min_cooldown(void) {
+static void test_alert_siren_30min_cooldown(void) {
@@ -387,3 +387,3 @@
-void test_alert_night_quiet_hours_mute(void) {
+static void test_alert_night_quiet_hours_mute(void) {
@@ -411,3 +411,3 @@
-void test_alert_battery_conservation_tier2(void) {
+static void test_alert_battery_conservation_tier2(void) {
@@ -436,3 +436,3 @@
-void test_alert_battery_critical_tier3(void) {
+static void test_alert_battery_critical_tier3(void) {
@@ -456,3 +456,3 @@
-void test_alert_sensor_fault_preemption(void) {
+static void test_alert_sensor_fault_preemption(void) {
@@ -470,3 +470,3 @@
-void test_alert_active_rain_preemption(void) {
+static void test_alert_active_rain_preemption(void) {
@@ -483,3 +483,3 @@
-void test_alert_full_storm_lifecycle_sim(void) {
+static void test_alert_full_storm_lifecycle_sim(void) {
@@ -513,3 +513,3 @@
-void test_alert_defensive_null_and_guards(void) {
+static void test_alert_defensive_null_and_guards(void) {
```

---

## 4. Verification & Prevention Guidelines

### Verification Checklist

- [x] All test functions in `tests/unit/test_alert_manager.c` possess `static void` linkage.
- [x] Host test compilation succeeds with `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Werror`.
- [x] All 24 test cases pass with 100% assertion success rate.

### Prevention Guidelines

1. **Unity Test Prototype Convention**:
   - In all ThrowTheSwitch Unity test source files (`tests/unit/test_*.c`), every test case function must be declared `static void test_<name>(void)` to avoid polluting global symbol namespace and triggering `-Wmissing-prototypes`.
2. **Local Pre-Commit Strict Diagnostics**:
   - Always compile with `-Wmissing-prototypes -Werror` before submitting code to CI.
