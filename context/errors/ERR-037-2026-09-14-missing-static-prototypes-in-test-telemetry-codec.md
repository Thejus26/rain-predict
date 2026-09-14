# Error Report: ERR-037 - Missing static Function Prototypes in test_telemetry_codec.c

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-037` |
| **Date & Time** | 2026-09-14 17:33:00 +05:30 |
| **Commit SHA** | [`41a32b2`](https://github.com/Thejus26/rain-predict/commit/41a32b2941f78347b2e2c02f70249505b187e19c) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Wmissing-prototypes, -Werror) |
| **Sprint & Task** | Sprint 5 (`S5-T1.3` Telemetry Codec Unit Test Suite) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=missing-prototypes`) |
| **Impacted Files** | [`tests/unit/test_telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_telemetry_codec.c)<br>[`firmware/middleware/inc/telemetry_codec.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/telemetry_codec.h)<br>[`firmware/middleware/src/telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/telemetry_codec.c) |

---

## 1. Description & Symptoms

During CI host test compilation (`ninja` with `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror`), compilation of `test_telemetry_codec.c` failed with 13 `-Wmissing-prototypes` errors:

```text
[49/83] Building C object tests/CMakeFiles/test_telemetry_codec.dir/unit/test_telemetry_codec.c.o
FAILED: [code=1] tests/CMakeFiles/test_telemetry_codec.dir/unit/test_telemetry_codec.c.o 
/usr/bin/cc -DUNITY_INCLUDE_CONFIG_H -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/tests/mocks -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_telemetry_codec.dir/unit/test_telemetry_codec.c.o -MF tests/CMakeFiles/test_telemetry_codec.dir/unit/test_telemetry_codec.c.o.d -o tests/CMakeFiles/test_telemetry_codec.dir/unit/test_telemetry_codec.c.o -c /home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:31:6: error: no previous prototype for ‘test_periodic_codec_null_guards’ [-Werror=missing-prototypes]
   31 | void test_periodic_codec_null_guards(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:51:6: error: no previous prototype for ‘test_periodic_codec_buffer_underflow’ [-Werror=missing-prototypes]
   51 | void test_periodic_codec_buffer_underflow(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:69:6: error: no previous prototype for ‘test_periodic_encode_nominal_daytime_hex_match’ [-Werror=missing-prototypes]
   69 | void test_periodic_encode_nominal_daytime_hex_match(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:101:6: error: no previous prototype for ‘test_periodic_encode_subzero_temperature’ [-Werror=missing-prototypes]
  101 | void test_periodic_encode_subzero_temperature(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:141:6: error: no previous prototype for ‘test_periodic_encode_bitfield_masks’ [-Werror=missing-prototypes]
  141 | void test_periodic_encode_bitfield_masks(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:182:6: error: no previous prototype for ‘test_periodic_encode_upper_boundary_clamping’ [-Werror=missing-prototypes]
  182 | void test_periodic_encode_upper_boundary_clamping(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:221:6: error: no previous prototype for ‘test_periodic_encode_lower_boundary_clamping’ [-Werror=missing-prototypes]
  221 | void test_periodic_encode_lower_boundary_clamping(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:259:6: error: no previous prototype for ‘test_periodic_roundtrip_lossless_fidelity’ [-Werror=missing-prototypes]
  259 | void test_periodic_roundtrip_lossless_fidelity(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:309:6: error: no previous prototype for ‘test_alert_codec_null_guards’ [-Werror=missing-prototypes]
  309 | void test_alert_codec_null_guards(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:327:6: error: no previous prototype for ‘test_alert_codec_buffer_underflow’ [-Werror=missing-prototypes]
  327 | void test_alert_codec_buffer_underflow(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:344:6: error: no previous prototype for ‘test_alert_encode_severe_storm_hex_match’ [-Werror=missing-prototypes]
  344 | void test_alert_encode_severe_storm_hex_match(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:373:6: error: no previous prototype for ‘test_alert_encode_signed_pressure_rate’ [-Werror=missing-prototypes]
  373 | void test_alert_encode_signed_pressure_rate(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_telemetry_codec.c:409:6: error: no previous prototype for ‘test_alert_roundtrip_lossless_fidelity’ [-Werror=missing-prototypes]
  409 | void test_alert_roundtrip_lossless_fidelity(void) {
      |      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cc1: all warnings being treated as errors
[50/83] Building C object tests/CMakeFiles/test_modbus_rtu.dir/unit/test_modbus_rtu.c.o
[51/83] Building C object tests/CMakeFiles/test_sdi12.dir/unit/test_sdi12.c.o
[52/83] Linking C static library tests/libunity.a
[53/83] Linking C static library firmware/core/libfirmware_core.a
ninja: build stopped: subcommand failed.
Error: Process completed with exit code 1.
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_telemetry_codec.c), all 13 unit test functions were declared with global external linkage (`void test_...`) without forward declarations:
- `test_periodic_codec_null_guards`
- `test_periodic_codec_buffer_underflow`
- `test_periodic_encode_nominal_daytime_hex_match`
- `test_periodic_encode_subzero_temperature`
- `test_periodic_encode_bitfield_masks`
- `test_periodic_encode_upper_boundary_clamping`
- `test_periodic_encode_lower_boundary_clamping`
- `test_periodic_roundtrip_lossless_fidelity`
- `test_alert_codec_null_guards`
- `test_alert_codec_buffer_underflow`
- `test_alert_encode_severe_storm_hex_match`
- `test_alert_encode_signed_pressure_rate`
- `test_alert_roundtrip_lossless_fidelity`

Under the project's strict compiler flag configuration (`-std=c99 -Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror`), GCC enforces that every function with external linkage must be preceded by a prototype declaration in an included header or file scope. Because these functions are only referenced locally within the test file via `RUN_TEST` inside `main()`, they should be restricted to internal linkage using the `static` storage-class specifier.

---

## 3. Resolution & Code Changes

### Resolution Steps

1. Added `static` storage-class specifiers to all 13 unit test function definitions in [`tests/unit/test_telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_telemetry_codec.c):
   - `static void test_periodic_codec_null_guards(void)`
   - `static void test_periodic_codec_buffer_underflow(void)`
   - `static void test_periodic_encode_nominal_daytime_hex_match(void)`
   - `static void test_periodic_encode_subzero_temperature(void)`
   - `static void test_periodic_encode_bitfield_masks(void)`
   - `static void test_periodic_encode_upper_boundary_clamping(void)`
   - `static void test_periodic_encode_lower_boundary_clamping(void)`
   - `static void test_periodic_roundtrip_lossless_fidelity(void)`
   - `static void test_alert_codec_null_guards(void)`
   - `static void test_alert_codec_buffer_underflow(void)`
   - `static void test_alert_encode_severe_storm_hex_match(void)`
   - `static void test_alert_encode_signed_pressure_rate(void)`
   - `static void test_alert_roundtrip_lossless_fidelity(void)`
2. Retained external linkage for Unity lifecycle hooks `setUp()` and `tearDown()` (prototyped by `unity.h`) and `main()`.

### Code Diff
```diff
diff --git a/tests/unit/test_telemetry_codec.c b/tests/unit/test_telemetry_codec.c
index e568126..68efcf0 100644
--- a/tests/unit/test_telemetry_codec.c
+++ b/tests/unit/test_telemetry_codec.c
@@ -28,7 +28,7 @@ void tearDown(void) {
 /**
  * @brief Verify NULL pointer guards on periodic encoder and decoder.
  */
-void test_periodic_codec_null_guards(void) {
+static void test_periodic_codec_null_guards(void) {
     telemetry_periodic_data_t data;
     uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
     size_t encoded_len = 0;
@@ -48,7 +48,7 @@ void test_periodic_codec_null_guards(void) {
 /**
  * @brief Verify buffer bounds rejection for truncated destination arrays.
  */
-void test_periodic_codec_buffer_underflow(void) {
+static void test_periodic_codec_buffer_underflow(void) {
     telemetry_periodic_data_t data;
     uint8_t buffer[TELEMETRY_PERIODIC_PAYLOAD_SIZE];
     size_t encoded_len = 0;
@@ -66,7 +66,7 @@ void test_periodic_codec_buffer_underflow(void) {
 /**
  * @brief Test exact hexadecimal bit-match for nominal tropical daytime profile.
  */
-void test_periodic_encode_nominal_daytime_hex_match(void) {
+static void test_periodic_encode_nominal_daytime_hex_match(void) {
     telemetry_periodic_data_t data = {
         .temperature_c          = 24.50f,
         .humidity_pct           = 85.25f,
@@ -98,7 +98,7 @@ void test_periodic_encode_nominal_daytime_hex_match(void) {
 /**
  * @brief Test sub-zero temperature two's-complement sign preservation.
  */
-void test_periodic_encode_subzero_temperature(void) {
+static void test_periodic_encode_subzero_temperature(void) {
     telemetry_periodic_data_t data = {
         .temperature_c          = -12.75f,
         .humidity_pct           = 99.90f,
@@ -138,7 +138,7 @@ void test_periodic_encode_subzero_temperature(void) {
 /**
  * @brief Test bitfield packing, flag masking, and isolation in Bytes 9, 10, and 11.
  */
-void test_periodic_encode_bitfield_masks(void) {
+static void test_periodic_encode_bitfield_masks(void) {
     telemetry_periodic_data_t data = {
         .temperature_c          = 20.00f,
         .humidity_pct           = 50.00f,
@@ -179,7 +179,7 @@ void test_periodic_encode_bitfield_masks(void) {
 /**
  * @brief Test maximum physical boundary clamping (no integer wrap-around).
  */
-void test_periodic_encode_upper_boundary_clamping(void) {
+static void test_periodic_encode_upper_boundary_clamping(void) {
     telemetry_periodic_data_t data = {
         .temperature_c          = 150.0f,    /* Exceeds 85.0 °C */
         .humidity_pct           = 120.0f,    /* Exceeds 100.0 % */
@@ -218,7 +218,7 @@ void test_periodic_encode_upper_boundary_clamping(void) {
 /**
  * @brief Test minimum physical boundary clamping (no integer underflow).
  */
-void test_periodic_encode_lower_boundary_clamping(void) {
+static void test_periodic_encode_lower_boundary_clamping(void) {
     telemetry_periodic_data_t data = {
         .temperature_c          = -80.0f,   /* Below -40.0 °C */
         .humidity_pct           = -20.0f,   /* Below 0.0 % */
@@ -256,7 +256,7 @@ void test_periodic_encode_lower_boundary_clamping(void) {
 /**
  * @brief Test comprehensive lossless round-trip encoding and decoding.
  */
-void test_periodic_roundtrip_lossless_fidelity(void) {
+static void test_periodic_roundtrip_lossless_fidelity(void) {
     const float test_temps[] = { -39.99f, -10.50f, 0.00f, 15.33f, 28.75f, 42.10f, 84.99f };
     const float test_pressures[] = { 300.00f, 650.40f, 950.22f, 1013.25f, 1099.98f };
     const float test_humidities[] = { 0.00f, 25.40f, 65.50f, 92.80f, 100.00f };
@@ -306,7 +306,7 @@ void test_periodic_roundtrip_lossless_fidelity(void) {
 /**
  * @brief Verify NULL pointer guards on alert encoder and decoder.
  */
-void test_alert_codec_null_guards(void) {
+static void test_alert_codec_null_guards(void) {
     telemetry_alert_data_t data;
     uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
     size_t encoded_len = 0;
@@ -324,7 +324,7 @@ void test_alert_codec_null_guards(void) {
 /**
  * @brief Verify buffer bounds rejection for truncated alert destination arrays.
  */
-void test_alert_codec_buffer_underflow(void) {
+static void test_alert_codec_buffer_underflow(void) {
     telemetry_alert_data_t data;
     uint8_t buffer[TELEMETRY_ALERT_PAYLOAD_SIZE];
     size_t encoded_len = 0;
@@ -341,7 +341,7 @@ void test_alert_codec_buffer_underflow(void) {
 /**
  * @brief Test exact hexadecimal bit-match for severe convective storm alert.
  */
-void test_alert_encode_severe_storm_hex_match(void) {
+static void test_alert_encode_severe_storm_hex_match(void) {
     telemetry_alert_data_t data = {
         .alert_state            = RAIN_ALERT_IMMINENT,
         .trigger_cause          = ALERT_TRIGGER_CPI_THRESHOLD,
@@ -370,7 +370,7 @@ void test_alert_encode_severe_storm_hex_match(void) {
 /**
  * @brief Test signed pressure rate quantization, negative drop rates, and clamping.
  */
-void test_alert_encode_signed_pressure_rate(void) {
+static void test_alert_encode_signed_pressure_rate(void) {
     telemetry_alert_data_t data = {
         .alert_state            = RAIN_ALERT_IMMINENT,
         .trigger_cause          = ALERT_TRIGGER_PRESSURE_PLUNGE,
@@ -406,7 +406,7 @@ void test_alert_encode_signed_pressure_rate(void) {
 /**
  * @brief Test full alert round-trip encoding and decoding across all enum states.
  */
-void test_alert_roundtrip_lossless_fidelity(void) {
+static void test_alert_roundtrip_lossless_fidelity(void) {
     for (uint8_t state = 0; state <= 3; state++) {
         for (uint8_t trig = 0; trig <= 7; trig++) {
             telemetry_alert_data_t src = {
```

---

## 4. Verification & Prevention Guidelines

1. **Internal Linkage for Unity Test Cases**:
   - Always declare all test case functions with `static void test_<name>(void)` in Unity unit test source files.
   - External linkage should only be used when a function is part of a public API defined in a corresponding `.h` header.
2. **Adherence to Coding Standards**:
   - Strictly follow [`context/coding-standards.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/coding-standards.md) Rule 3 on scope and linkage control.
3. **Compiler Flag Parity**:
   - Ensure local developer builds run with `-Wall -Wextra -Wpedantic -Wstrict-prototypes -Wmissing-prototypes -Werror` to mirror GitHub Actions CI environments before submitting commits.
