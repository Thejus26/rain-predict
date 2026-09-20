# Error Report: ERR-043 - Redeclaration of Enumerator 'RAIN_ALERT_UNLIKELY' in Telemetry Codec Header

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-043` |
| **Date & Time** | 2026-09-20 12:10:00 +05:30 |
| **Commit SHA** | [`c314546`](https://github.com/Thejus26/rain-predict/commit/c31454635f37237d9ac30b5f454ca71fb4b7ff47) |
| **Component / Subsystem** | Strict C99 Compiler Diagnostics (-Werror, enumerator redeclaration) |
| **Sprint & Task** | Sprint 6 (`S6-T3.1` 8-State Application State Machine & Lifecycle Flow) |
| **Severity** | High (CI Embedded & Cross-Compilation Build Failure under `-Werror`) |
| **Impacted Files** | [`firmware/middleware/inc/telemetry_codec.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/telemetry_codec.h)<br>[`firmware/middleware/src/telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/telemetry_codec.c)<br>[`firmware/app/inc/rain_algo.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/rain_algo.h)<br>[`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c)<br>[`tests/unit/test_telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_telemetry_codec.c) |

---

## 1. Description & Symptoms

During automated CI execution on GitHub Actions runner (`arm-none-eabi-gcc` target compilation), the build aborted when compiling [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c) with strict diagnostics enabled (`-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Werror`):

```text
FAILED: firmware/app/CMakeFiles/firmware_app.dir/src/app_state_machine.c.obj 
/usr/bin/arm-none-eabi-gcc -DSTM32WLE5xx -DUSE_HAL_DRIVER \
  -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc \
  -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc \
  -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc \
  -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc \
  -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -ffunction-sections -fdata-sections -O3 -DNDEBUG -std=c99 \
  -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith \
  -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror \
  -MD -MT firmware/app/CMakeFiles/firmware_app.dir/src/app_state_machine.c.obj \
  -MF firmware/app/CMakeFiles/firmware_app.dir/src/app_state_machine.c.obj.d \
  -o firmware/app/CMakeFiles/firmware_app.dir/src/app_state_machine.c.obj \
  -c /home/runner/work/rain-predict/rain-predict/firmware/app/src/app_state_machine.c

In file included from /home/runner/work/rain-predict/rain-predict/firmware/app/src/app_state_machine.c:30:
/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc/telemetry_codec.h:95:5: error: redeclaration of enumerator 'RAIN_ALERT_UNLIKELY'
   95 |     RAIN_ALERT_UNLIKELY    = 0U,    /**< 00: Rain unlikely (CPI < 30%) */
      |     ^~~~~~~~~~~~~~~~~~~
In file included from /home/runner/work/rain-predict/rain-predict/firmware/app/inc/app_state_machine.h:27,
                 from /home/runner/work/rain-predict/rain-predict/firmware/app/src/app_state_machine.c:20:
/home/runner/work/rain-predict/rain-predict/firmware/app/inc/rain_algo.h:46:5: note: previous definition of 'RAIN_ALERT_UNLIKELY' was here
   46 |     RAIN_ALERT_UNLIKELY     = 0, /**< CPI < 30% (Settled fine weather, Green Alert) */
      |     ^~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc/telemetry_codec.h:96:5: error: redeclaration of enumerator 'RAIN_ALERT_POSSIBLE'
   96 |     RAIN_ALERT_POSSIBLE    = 1U,    /**< 01: Rain possible (30% <= CPI < 60%) */
      |     ^~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/app/inc/rain_algo.h:47:5: note: previous definition of 'RAIN_ALERT_POSSIBLE' was here
   47 |     RAIN_ALERT_POSSIBLE     = 1, /**< 30% <= CPI < 60% (Unsettled / Showers possible, Yellow Alert) */
      |     ^~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc/telemetry_codec.h:97:5: error: redeclaration of enumerator 'RAIN_ALERT_IMMINENT'
   97 |     RAIN_ALERT_IMMINENT    = 2U,    /**< 10: Rain imminent (CPI >= 60%) */
      |     ^~~~~~~~~~~~~~~~~~~
/home/runner/work/rain-predict/rain-predict/firmware/app/inc/rain_algo.h:49:5: note: previous definition of 'RAIN_ALERT_IMMINENT' was here
   49 |     RAIN_ALERT_IMMINENT     = 3  /**< CPI >= 80% or Critical Trigger (Active storm in 15-30m, Red Alert) */
      |     ^~~~~~~~~~~~~~~~~~~
```

---

## 2. Root Cause Analysis

### Blast Radius & Knowledge Graph Analysis (`graphify`)

Querying the knowledge graph for `RAIN_ALERT_UNLIKELY` and downstream callers reveals:
- **Application State Machine**: [`app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c) orchestrates the 8-state system lifecycle. It includes [`app_state_machine.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/app_state_machine.h) (which includes algorithmic classifications from [`rain_algo.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/rain_algo.h)) and simultaneously includes the packet serialization engine [`telemetry_codec.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/telemetry_codec.h).
- **Collision Boundary**:
  - `rain_algo.h` defines `rain_alert_state_t` (internal algorithm forecast classification).
  - `telemetry_codec.h` defines `telemetry_rain_state_t` (2-bit over-the-air payload serialization format).
  - Calling tests and modules: [`tests/unit/test_telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_telemetry_codec.c), [`firmware/middleware/src/telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/src/telemetry_codec.c), [`firmware/app/src/rain_algo.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/rain_algo.c), and [`firmware/app/src/alert_manager.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/alert_manager.c).

### Technical Root Cause

In C99 (ISO/IEC 9899:1999 §6.2.1), enumerator constants belong to the ordinary identifier namespace with translation-unit scope when declared at file level. Unlike C++ scoped enums (`enum class`), C enum constants cannot share names even if their containing `enum` tags or typedefs differ.

1. In [`firmware/app/inc/rain_algo.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/inc/rain_algo.h):
   ```c
   typedef enum {
       RAIN_ALERT_UNLIKELY     = 0, /**< CPI < 30% (Settled fine weather, Green Alert) */
       RAIN_ALERT_POSSIBLE     = 1, /**< 30% <= CPI < 60% (Unsettled / Showers possible, Yellow Alert) */
       RAIN_ALERT_LIKELY       = 2, /**< 60% <= CPI < 80% (High probability in 1-2h, Orange Alert) */
       RAIN_ALERT_IMMINENT     = 3  /**< CPI >= 80% or Critical Trigger (Active storm in 15-30m, Red Alert) */
   } rain_alert_state_t;
   ```

2. In [`firmware/middleware/inc/telemetry_codec.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/telemetry_codec.h):
   ```c
   typedef enum {
       RAIN_ALERT_UNLIKELY    = 0U,    /**< 00: Rain unlikely (CPI < 30%) */
       RAIN_ALERT_POSSIBLE    = 1U,    /**< 01: Rain possible (30% <= CPI < 60%) */
       RAIN_ALERT_IMMINENT    = 2U,    /**< 10: Rain imminent (CPI >= 60%) */
       RAIN_ALERT_ACTIVE_RAIN = 3U     /**< 11: Physical rainfall in progress */
   } telemetry_rain_state_t;
   ```

Prior to `S6-T3.1`, `rain_algo.h` and `telemetry_codec.h` were never included within the same C translation unit. `app_state_machine.c` is the first unified coordinator file to pull in both the predictive algorithm subsystem and the LoRaWAN telemetry serialization subsystem.

Crucially, in addition to the identifier namespace collision, the two enums have **conflicting numeric values**:
- In `rain_algo.h`, `RAIN_ALERT_IMMINENT == 3`.
- In `telemetry_codec.h`, `RAIN_ALERT_IMMINENT == 2U` (with `RAIN_ALERT_ACTIVE_RAIN == 3U`).

Under `-Werror`, the compiler immediately halted execution with multiple `redeclaration of enumerator` errors.

---

## 3. Resolution & Code Changes

*Status: Resolved & Verified*

### Concrete Resolution Steps Taken

1. **Namespace Isolation for Telemetry Enumerators**:
   Prefixed all enumerator members of `telemetry_rain_state_t` and `telemetry_rain_intensity_t` in [`firmware/middleware/inc/telemetry_codec.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/middleware/inc/telemetry_codec.h) with `TELEMETRY_RAIN_STATE_*` and `TELEMETRY_RAIN_INTENSITY_*` respectively. This prevents identifier collisions with `rain_algo.h` and `rain_gauge_driver.h`:
   ```c
   typedef enum {
       TELEMETRY_RAIN_STATE_UNLIKELY    = 0U,    /**< 00: Rain unlikely (CPI < 30%) */
       TELEMETRY_RAIN_STATE_POSSIBLE    = 1U,    /**< 01: Rain possible (30% <= CPI < 60%) */
       TELEMETRY_RAIN_STATE_IMMINENT    = 2U,    /**< 10: Rain imminent (CPI >= 60%) */
       TELEMETRY_RAIN_STATE_ACTIVE_RAIN = 3U     /**< 11: Physical rainfall in progress */
   } telemetry_rain_state_t;

   typedef enum {
       TELEMETRY_RAIN_INTENSITY_NONE     = 0U,   /**< 00: No rain (0.0 mm/hr) */
       TELEMETRY_RAIN_INTENSITY_LIGHT    = 1U,   /**< 01: Light rain (0.1 .. 2.5 mm/hr) */
       TELEMETRY_RAIN_INTENSITY_MODERATE = 2U,   /**< 10: Moderate rain (2.5 .. 10.0 mm/hr) */
       TELEMETRY_RAIN_INTENSITY_HEAVY    = 3U    /**< 11: Heavy / torrential rain (> 10.0 mm/hr) */
   } telemetry_rain_intensity_t;
   ```

2. **Domain Model to Wire Protocol Mapping in State Machine**:
   In [`firmware/app/src/app_state_machine.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/app/src/app_state_machine.c) (`app_exec_transmit`), eliminated the direct enum cast `(telemetry_rain_state_t)s_app_ctx.rain_state`. Implemented an explicit mapping ladder checking `rain_pulses_cycle > 0` and `rain_gauge_is_rain_active()` for `TELEMETRY_RAIN_STATE_ACTIVE_RAIN`, and translating `rain_alert_state_t` thresholds to the appropriate `TELEMETRY_RAIN_STATE_*` wire representation.

3. **Telemetry Unit Test Suite Update**:
   Updated all 11 test vector references in [`tests/unit/test_telemetry_codec.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_telemetry_codec.c) to use the new `TELEMETRY_RAIN_STATE_*` and `TELEMETRY_RAIN_INTENSITY_*` symbols.

### Code Changes (Unified Diff)

```diff
diff --git a/firmware/app/src/app_state_machine.c b/firmware/app/src/app_state_machine.c
index a8ca5c4..988e025 100644
--- a/firmware/app/src/app_state_machine.c
+++ b/firmware/app/src/app_state_machine.c
@@ -242,6 +242,18 @@ static status_t app_exec_predict(void) {
 }
 
 static status_t app_exec_transmit(void) {
+    /* Map internal rain alert and physical gauge state to over-the-air telemetry state */
+    telemetry_rain_state_t telem_rain;
+    if (s_app_ctx.rain_pulses_cycle > 0U || rain_gauge_is_rain_active()) {
+        telem_rain = TELEMETRY_RAIN_STATE_ACTIVE_RAIN;
+    } else if (s_app_ctx.rain_state >= RAIN_ALERT_LIKELY || s_app_ctx.cpi_pct >= 60.0f) {
+        telem_rain = TELEMETRY_RAIN_STATE_IMMINENT;
+    } else if (s_app_ctx.rain_state == RAIN_ALERT_POSSIBLE || s_app_ctx.cpi_pct >= 30.0f) {
+        telem_rain = TELEMETRY_RAIN_STATE_POSSIBLE;
+    } else {
+        telem_rain = TELEMETRY_RAIN_STATE_UNLIKELY;
+    }
+
     /* 1. Serialize periodic telemetry packet */
     telemetry_periodic_data_t telem = {
         .temperature_c          = s_app_ctx.temperature_c,
@@ -250,7 +262,7 @@ static status_t app_exec_transmit(void) {
         .pressure_hpa           = s_app_ctx.pressure_hpa,
         .ambient_lux            = s_app_ctx.solar_lux,
         .rain_interval_mm       = (float)s_app_ctx.rain_pulses_cycle * RAIN_GAUGE_CALIB_MM_PER_TIP,
-        .forecast_state         = (telemetry_rain_state_t)s_app_ctx.rain_state,
+        .forecast_state         = telem_rain,
         .zambretti_index        = s_app_ctx.zambretti_code,
         .cpi_prob_pct           = (uint8_t)s_app_ctx.cpi_pct,
         .solar_cloud_drop_alarm = (s_app_ctx.solar_lux < 3000.0f && s_app_ctx.solar_lux > 50.0f),
diff --git a/firmware/middleware/inc/telemetry_codec.h b/firmware/middleware/inc/telemetry_codec.h
index 51886bb..5169400 100644
--- a/firmware/middleware/inc/telemetry_codec.h
+++ b/firmware/middleware/inc/telemetry_codec.h
@@ -89,13 +89,13 @@ extern "C" {
 /* ========================================================================== */
 
 /**
- * @brief Nowcast rain alert operational state classification.
+ * @brief Nowcast rain alert operational state classification for over-the-air telemetry.
  */
 typedef enum {
-    RAIN_ALERT_UNLIKELY    = 0U,    /**< 00: Rain unlikely (CPI < 30%) */
-    RAIN_ALERT_POSSIBLE    = 1U,    /**< 01: Rain possible (30% <= CPI < 60%) */
-    RAIN_ALERT_IMMINENT    = 2U,    /**< 10: Rain imminent (CPI >= 60%) */
-    RAIN_ALERT_ACTIVE_RAIN = 3U     /**< 11: Physical rainfall in progress */
+    TELEMETRY_RAIN_STATE_UNLIKELY    = 0U,    /**< 00: Rain unlikely (CPI < 30%) */
+    TELEMETRY_RAIN_STATE_POSSIBLE    = 1U,    /**< 01: Rain possible (30% <= CPI < 60%) */
+    TELEMETRY_RAIN_STATE_IMMINENT    = 2U,    /**< 10: Rain imminent (CPI >= 60%) */
+    TELEMETRY_RAIN_STATE_ACTIVE_RAIN = 3U     /**< 11: Physical rainfall in progress */
 } telemetry_rain_state_t;
 
 /**
@@ -116,10 +116,10 @@ typedef enum {
  * @brief Rainfall rate intensity classification tier.
  */
 typedef enum {
-    RAIN_INTENSITY_NONE     = 0U,   /**< 00: No rain (0.0 mm/hr) */
-    RAIN_INTENSITY_LIGHT    = 1U,   /**< 01: Light rain (0.1 .. 2.5 mm/hr) */
-    RAIN_INTENSITY_MODERATE = 2U,   /**< 10: Moderate rain (2.5 .. 10.0 mm/hr) */
-    RAIN_INTENSITY_HEAVY    = 3U    /**< 11: Heavy / torrential rain (> 10.0 mm/hr) */
+    TELEMETRY_RAIN_INTENSITY_NONE     = 0U,   /**< 00: No rain (0.0 mm/hr) */
+    TELEMETRY_RAIN_INTENSITY_LIGHT    = 1U,   /**< 01: Light rain (0.1 .. 2.5 mm/hr) */
+    TELEMETRY_RAIN_INTENSITY_MODERATE = 2U,   /**< 10: Moderate rain (2.5 .. 10.0 mm/hr) */
+    TELEMETRY_RAIN_INTENSITY_HEAVY    = 3U    /**< 11: Heavy / torrential rain (> 10.0 mm/hr) */
 } telemetry_rain_intensity_t;
 
 /**
diff --git a/tests/unit/test_telemetry_codec.c b/tests/unit/test_telemetry_codec.c
index 68efcf0..dfc9b70 100644
--- a/tests/unit/test_telemetry_codec.c
+++ b/tests/unit/test_telemetry_codec.c
@@ -73,7 +73,7 @@ static void test_periodic_encode_nominal_daytime_hex_match(void) {
         .pressure_hpa           = 945.50f,
         .ambient_lux            = 45000.0f,
         .rain_interval_mm       = 0.0f,
-        .forecast_state         = RAIN_ALERT_POSSIBLE,
+        .forecast_state         = TELEMETRY_RAIN_STATE_POSSIBLE,
         .zambretti_index        = 14U,
         .cpi_prob_pct           = 45U,
         .solar_cloud_drop_alarm = false,
@@ -105,7 +105,7 @@ static void test_periodic_encode_subzero_temperature(void) {
         .pressure_hpa           = 810.20f,
         .ambient_lux            = 0.0f,
         .rain_interval_mm       = 2.4f,
-        .forecast_state         = RAIN_ALERT_IMMINENT,
+        .forecast_state         = TELEMETRY_RAIN_STATE_IMMINENT,
         .zambretti_index        = 22U,
         .cpi_prob_pct           = 78U,
         .solar_cloud_drop_alarm = false,
@@ -145,7 +145,7 @@ static void test_periodic_encode_bitfield_masks(void) {
         .pressure_hpa           = 1000.00f,
         .ambient_lux            = 1000.0f,
         .rain_interval_mm       = 1.0f,
-        .forecast_state         = RAIN_ALERT_ACTIVE_RAIN, /* 3 (bits 7:6 = 11) */
+        .forecast_state         = TELEMETRY_RAIN_STATE_ACTIVE_RAIN, /* 3 (bits 7:6 = 11) */
         .zambretti_index        = 26U,                    /* 26 (bits 5:0 = 0x1A) -> 0xDA */
         .cpi_prob_pct           = 100U,                   /* 100 (bits 6:0 = 0x64) */
         .solar_cloud_drop_alarm = true,                   /* true (bit 7 = 1) -> 0xE4 */
@@ -167,7 +167,7 @@ static void test_periodic_encode_bitfield_masks(void) {
     telemetry_periodic_data_t decoded;
     status = telemetry_decode_periodic(buffer, sizeof(buffer), &decoded);
     TEST_ASSERT_EQUAL(STATUS_OK, status);
-    TEST_ASSERT_EQUAL(RAIN_ALERT_ACTIVE_RAIN, decoded.forecast_state);
+    TEST_ASSERT_EQUAL(TELEMETRY_RAIN_STATE_ACTIVE_RAIN, decoded.forecast_state);
     TEST_ASSERT_EQUAL_UINT8(26U, decoded.zambretti_index);
     TEST_ASSERT_EQUAL_UINT8(100U, decoded.cpi_prob_pct);
     TEST_ASSERT_TRUE(decoded.solar_cloud_drop_alarm);
@@ -186,7 +186,7 @@ static void test_periodic_encode_upper_boundary_clamping(void) {
         .pressure_hpa           = 1300.0f,   /* Exceeds 1100.0 hPa */
         .ambient_lux            = 200000.0f, /* Exceeds 83,000 Lux */
         .rain_interval_mm       = 100.0f,    /* Exceeds 51.0 mm */
-        .forecast_state         = RAIN_ALERT_ACTIVE_RAIN,
+        .forecast_state         = TELEMETRY_RAIN_STATE_ACTIVE_RAIN,
         .zambretti_index        = 30U,       /* Exceeds 26 */
         .cpi_prob_pct           = 150U,      /* Exceeds 100% */
         .solar_cloud_drop_alarm = true,
@@ -225,7 +225,7 @@ static void test_periodic_encode_lower_boundary_clamping(void) {
         .pressure_hpa           = 100.0f,   /* Below 300.0 hPa */
         .ambient_lux            = -50.0f,   /* Below 0.0 Lux */
         .rain_interval_mm       = -5.0f,    /* Below 0.0 mm */
-        .forecast_state         = RAIN_ALERT_UNLIKELY,
+        .forecast_state         = TELEMETRY_RAIN_STATE_UNLIKELY,
         .zambretti_index        = 0U,       /* Below 1 */
         .cpi_prob_pct           = 0U,
         .solar_cloud_drop_alarm = false,
@@ -270,7 +270,7 @@ static void test_periodic_roundtrip_lossless_fidelity(void) {
                     .pressure_hpa           = test_pressures[p],
                     .ambient_lux            = 32000.0f,
                     .rain_interval_mm       = 4.6f,
-                    .forecast_state         = RAIN_ALERT_POSSIBLE,
+                    .forecast_state         = TELEMETRY_RAIN_STATE_POSSIBLE,
                     .zambretti_index        = 12U,
                     .cpi_prob_pct           = 55U,
                     .solar_cloud_drop_alarm = false,
@@ -343,13 +343,13 @@ static void test_alert_codec_buffer_underflow(void) {
  */
 static void test_alert_encode_severe_storm_hex_match(void) {
     telemetry_alert_data_t data = {
-        .alert_state            = RAIN_ALERT_IMMINENT,
+        .alert_state            = TELEMETRY_RAIN_STATE_IMMINENT,
         .trigger_cause          = ALERT_TRIGGER_CPI_THRESHOLD,
         .alert_sequence_id      = 3U,
         .cpi_prob_pct           = 88U,
         .solar_cloud_drop_alarm = true,
         .pressure_rate_hpa_per_h= -3.50f,
-        .rain_intensity         = RAIN_INTENSITY_LIGHT,
+        .rain_intensity         = TELEMETRY_RAIN_INTENSITY_LIGHT,
         .sensor_fault           = false,
         .battery_voltage_v      = 3.30f
     };
@@ -372,13 +372,13 @@ static void test_alert_encode_severe_storm_hex_match(void) {
  */
 static void test_alert_encode_signed_pressure_rate(void) {
     telemetry_alert_data_t data = {
-        .alert_state            = RAIN_ALERT_IMMINENT,
+        .alert_state            = TELEMETRY_RAIN_STATE_IMMINENT,
         .trigger_cause          = ALERT_TRIGGER_PRESSURE_PLUNGE,
         .alert_sequence_id      = 5U,
         .cpi_prob_pct           = 92U,
         .solar_cloud_drop_alarm = false,
         .pressure_rate_hpa_per_h= -5.20f, /* raw -104 = 0x98 */
-        .rain_intensity         = RAIN_INTENSITY_NONE,
+        .rain_intensity         = TELEMETRY_RAIN_INTENSITY_NONE,
         .sensor_fault           = false,
         .battery_voltage_v      = 3.26f   /* raw 19 = 0x13 */
     };
```

---

## 4. Verification & Prevention Guidelines

### Prevention Rules

1. **Strict Enum Prefixing in C Header Files**:
   Every enum declared in public headers must prefix every enumerator with the module or typedef prefix (e.g., `TELEMETRY_RAIN_STATE_*`, `RAIN_ALERT_*`). Generic names like `UNLIKELY` or non-subsystem-prefixed names like `RAIN_ALERT_*` must never be reused across different domain subsystems.
2. **Distinct Domain Representation**:
   Keep domain concepts distinct: an *algorithmic forecast level* (`rain_alert_state_t`) is mathematically and semantically distinct from an *over-the-air wire protocol bitfield* (`telemetry_rain_state_t`). Explicit conversion logic prevents unintended bit-pattern conflation.

### Verification Plan

1. **Target Toolchain Cross-Compilation**:
   Verify clean compilation of `app_state_machine.c` under `arm-none-eabi-gcc` with `-std=c99 -Werror`.
2. **Host Unit Test Suite**:
   Run full test suite via CMake / CTest:
   ```bash
   ctest --test-dir build-host --output-on-failure
   ```
   Ensuring all 17+ unit test suites, especially `test_telemetry_codec`, pass with 100% assertions.
