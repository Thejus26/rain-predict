# Error Report: ERR-058 - Redundant setUp/tearDown Redeclarations & Undeclared TEST_ASSERT_NOT_EQUAL in Calibration Unit Tests

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-058` |
| **Date & Time** | 2026-09-21 09:14:32 +05:30 |
| **Commit SHA** | [`38e5d0a`](https://github.com/Thejus26/rain-predict/commit/38e5d0a9c86d60521ae6952e41f1fad340a0c6d4) |
| **Component / Subsystem** | Strict C99 & Unity Framework Diagnostics |
| **Sprint & Task** | Sprint 7 (`S7-T3.1` Barometric Altitude Calibration & `S7-T3.2` Tipping-Bucket Rain Gauge Calibration) |
| **Severity** | High (CI Host Compilation Failure under `-Werror=redundant-decls` and `-Werror=implicit-function-declaration`) |
| **Impacted Files** | [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c)<br>[`tests/unit/test_rain_gauge_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_gauge_calibration.c) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI host test compilation (`/usr/bin/cc` with `-std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage`), compilation of [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c) failed with fatal compilation errors:

```text
FAILED: [code=1] tests/CMakeFiles/test_barometric_calibration.dir/unit/test_barometric_calibration.c.o 
/usr/bin/cc -DHOST_TEST=1 -DUNITY_INCLUDE_CONFIG_H=1 -I/home/runner/work/rain-predict/rain-predict/tests/unity -I/home/runner/work/rain-predict/rain-predict/firmware/core/inc -I/home/runner/work/rain-predict/rain-predict/firmware/drivers/inc -I/home/runner/work/rain-predict/rain-predict/firmware/middleware/inc -I/home/runner/work/rain-predict/rain-predict/firmware/app/inc -g -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wpointer-arith -Wundef -Wmissing-prototypes -Wredundant-decls -Wformat=2 -Wwrite-strings -Werror --coverage -O0 -g3 -MD -MT tests/CMakeFiles/test_barometric_calibration.dir/unit/test_barometric_calibration.c.o -MF tests/CMakeFiles/test_barometric_calibration.dir/unit/test_barometric_calibration.c.o.d -o tests/CMakeFiles/test_barometric_calibration.dir/unit/test_barometric_calibration.c.o -c /home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c
/home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c:148:6: error: redundant redeclaration of ‘setUp’ [-Werror=redundant-decls]
  148 | void setUp(void);
      |      ^~~~~
In file included from /home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c:21:
/home/runner/work/rain-predict/rain-predict/tests/unity/unity.h:25:6: note: previous declaration of ‘setUp’ with type ‘void(void)’
   25 | void setUp(void);
      |      ^~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c:149:6: error: redundant redeclaration of ‘tearDown’ [-Werror=redundant-decls]
  149 | void tearDown(void);
      |      ^~~~~
/home/runner/work/rain-predict/rain-predict/tests/unity/unity.h:26:6: note: previous declaration of ‘tearDown’ with type ‘void(void)’
   26 | void tearDown(void);
      |      ^~~~~
/home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c: In function ‘test_tc_cal_08_nvm_struct_crc_integrity’:
/home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c:274:5: error: implicit declaration of function ‘TEST_ASSERT_NOT_EQUAL’; did you mean ‘TEST_ASSERT_NOT_NULL’? [-Werror=implicit-function-declaration]
  274 |     TEST_ASSERT_NOT_EQUAL(0x0000U, cfg.crc16_checksum);
      |     ^~~~~~~~~~~~~~~~~~~~~
      |     TEST_ASSERT_NOT_NULL
cc1: all warnings being treated as errors
```

Furthermore, blast radius analysis revealed that [`tests/unit/test_rain_gauge_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_gauge_calibration.c) lines 107–108 contains the exact same redundant forward declarations of `setUp(void)` and `tearDown(void)` which would trigger identical `-Werror=redundant-decls` compiler failures.

---

## 2. Root Cause Analysis

Two distinct root causes triggered these compilation errors:

1. **Redundant Declarations of `setUp(void)` and `tearDown(void)` under `-Wredundant-decls`**:
   - The test harness header [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h) explicitly declares the test lifecycle hooks at lines 25–26:
     ```c
     void setUp(void);
     void tearDown(void);
     ```
   - In [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c) lines 148–149 and [`tests/unit/test_rain_gauge_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_gauge_calibration.c) lines 107–108, redundant forward declarations `void setUp(void);` and `void tearDown(void);` were explicitly reintroduced above their definitions.
   - Under GCC's strict `-Wredundant-decls -Werror` compiler flags, redeclaring a function already declared in an included header is treated as a fatal compilation error.

2. **Non-Existent Unity Macro `TEST_ASSERT_NOT_EQUAL`**:
   - In [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c) line 274:
     ```c
     TEST_ASSERT_NOT_EQUAL(0x0000U, cfg.crc16_checksum);
     ```
   - The embedded Unity distribution in this repository ([`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h)) does not implement `TEST_ASSERT_NOT_EQUAL`.
   - In C99, invoking an undeclared macro causes GCC to interpret it as an undeclared function returning `int`, triggering `-Werror=implicit-function-declaration`.
   - Canonical equality assertions in this repo's Unity configuration include `TEST_ASSERT_TRUE(condition)` or `TEST_ASSERT_FALSE(condition)`.

---

## 3. Resolution & Code Changes *(Implemented)*

### Fix Strategy

1. **Remove Redundant Declarations**:
   - Removed `void setUp(void);` and `void tearDown(void);` forward declarations from [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c).
   - Removed `void setUp(void);` and `void tearDown(void);` forward declarations from [`tests/unit/test_rain_gauge_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_rain_gauge_calibration.c).

2. **Replace `TEST_ASSERT_NOT_EQUAL` with Canonical Assertion**:
   - Replaced `TEST_ASSERT_NOT_EQUAL(0x0000U, cfg.crc16_checksum);` in [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c) with:
     ```c
     TEST_ASSERT_TRUE(cfg.crc16_checksum != 0x0000U);
     ```

### Code Changes

```diff
--- a/tests/unit/test_barometric_calibration.c
+++ b/tests/unit/test_barometric_calibration.c
@@ -145,8 +145,6 @@ calibration_status_t parse_lorawan_downlink_calibration(const uint8_t *payload,
 /* Forward Declarations for Static Test Functions                             */
 /* ========================================================================== */
 
-void setUp(void);
-void tearDown(void);
 static void test_tc_cal_01_munnar_reduction(void);
 static void test_tc_cal_02_nilgiris_reduction(void);
 static void test_tc_cal_03_assam_reduction(void);
@@ -271,7 +269,7 @@ static void test_tc_cal_08_nvm_struct_crc_integrity(void)
     uint16_t crc = calculate_crc16((const uint8_t *)&cfg + 8, sizeof(cfg) - 8U);
     cfg.crc16_checksum = crc;
 
-    TEST_ASSERT_NOT_EQUAL(0x0000U, cfg.crc16_checksum);
+    TEST_ASSERT_TRUE(cfg.crc16_checksum != 0x0000U);
     TEST_ASSERT_EQUAL_HEX16(crc, calculate_crc16((const uint8_t *)&cfg + 8, sizeof(cfg) - 8U));
 }

--- a/tests/unit/test_rain_gauge_calibration.c
+++ b/tests/unit/test_rain_gauge_calibration.c
@@ -104,8 +104,6 @@ rg_cal_status_t parse_lorawan_k_factor_downlink(const uint8_t *payload,
 /* Forward Declarations for Static Test Functions                             */
 /* ========================================================================== */
 
-void setUp(void);
-void tearDown(void);
 static void test_tc_rg_01_funnel_area(void);
 static void test_tc_rg_02_tip_volume(void);
 static void test_tc_rg_03_expected_tips_500ml(void);
```

---

## 4. Verification & Prevention Guidelines

1. **Unity Lifecycle Hook Forward Declarations**:
   - Never forward declare `setUp(void)` or `tearDown(void)` in test files. They are already declared in [`tests/unity/unity.h`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unity/unity.h). Only define their implementation.
2. **Canonical Unity Assertion Usage**:
   - Only use assertions provided by the project's Unity header (`TEST_ASSERT_TRUE`, `TEST_ASSERT_FALSE`, `TEST_ASSERT_EQUAL_INT`, `TEST_ASSERT_EQUAL_UINT16`, `TEST_ASSERT_FLOAT_WITHIN`, `TEST_ASSERT_EQUAL_HEX16`, etc.).
   - Do not use non-standard macros such as `TEST_ASSERT_NOT_EQUAL` or `TEST_ASSERT_TRUE_MESSAGE`.
3. **Multi-File Blast Radius Check**:
   - When introducing test suites with common boilerplate, ensure shared patterns (like forward declaration blocks) do not duplicate global declarations across adjacent test files.
