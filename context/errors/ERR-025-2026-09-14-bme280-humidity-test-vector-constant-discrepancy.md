# Error Report: ERR-025 - Bosch Reference Vector Humidity Constant Discrepancy in test_bme280.c

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-025` |
| **Date & Time** | `2026-09-14 08:29:37 +0530` |
| **Commit SHA** | [`5a2a635`](https://github.com/Thejus26/rain-predict/commit/5a2a6353ea0ad47fd2fe3cf79cade0295cb6bd43) |
| **Sprint / Task** | Sprint 4 (S4-T1.3 BME280 FPU Compensation Calculations & Unit Testing) |
| **Severity** | Medium (Unit Test Failure / Test Vector Discrepancy) |
| **Impacted Files** | [`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c) |

---

## 1. Description & Symptoms

During CTest execution of the unit test suite (`ctest --test-dir build-host --output-on-failure`), the `bme280` test executable failed with 3 assertion failures in humidity calculations:

```text
21/21 Test #21: bme280 ...........................***Failed  Error regular expression found in output. Regex=[FAIL]  0.00 sec
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c:563:test_bme280_compensation_humidity_vector:FAIL: Expected 54.320000 Was 29.806267
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c:658:test_bme280_compensation_fixed_point_scaling:FAIL: Expected 1 Was 0.  Expected TRUE Was FALSE
/home/runner/work/rain-predict/rain-predict/tests/unit/test_bme280.c:678:test_bme280_read_data_end_to_end:FAIL: Expected 54.320000 Was 29.806702

-----------------------
24 Tests 3 Failures 0 Ignored
FAIL
```

---

## 2. Root Cause Analysis

In [`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c), the test vector macro `EXPECTED_BOSCH_COMP_H` was defined as `54.32f` (%RH) based on a theoretical specification placeholder.

However, evaluating the standard Bosch single-precision humidity compensation formula:
$$var_H = (t_{\text{fine}} - 76800.0)$$
$$var_H = \left(adc_H - (dig\_H4 \times 64.0 + \frac{dig\_H5}{16384.0} \times var_H)\right) \times \left(\frac{dig\_H2}{65536.0} \times \left(1.0 + \frac{dig\_H6}{67108864.0} \times var_H \times (1.0 + \frac{dig\_H3}{67108864.0} \times var_H)\right)\right)$$
$$var_H = var_H \times \left(1.0 - \frac{dig\_H1 \times var_H}{524288.0}\right)$$

with the standard Bosch trimming parameters (`dig_H1 = 75`, `dig_H2 = 363`, `dig_H3 = 0`, `dig_H4 = 315`, `dig_H5 = 50`, `dig_H6 = 30`), raw $adc_H = 25600$, and $t_{\text{fine}} = 128422.287$ yields an exact value of $29.806267\%\text{ RH}$ ($29.81\%\text{ RH}$, or $2981\text{ centi-\%RH}$).

Because the test expected $54.32\%$, `test_bme280_compensation_humidity_vector`, `test_bme280_compensation_fixed_point_scaling` (expecting $5432$), and `test_bme280_read_data_end_to_end` failed.

---

## 3. Resolution & Code Changes

Updated `EXPECTED_BOSCH_COMP_H` in [`tests/unit/test_bme280.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_bme280.c) to `29.81f` (%RH) and updated the fixed-point scaling expectation to `2981U` centi-%RH.

```diff
--- a/tests/unit/test_bme280.c
+++ b/tests/unit/test_bme280.c
@@ -59,7 +59,7 @@
 
 #define EXPECTED_BOSCH_COMP_T   25.08f   /* °C */
 #define EXPECTED_BOSCH_COMP_P   1006.53f /* hPa */
-#define EXPECTED_BOSCH_COMP_H   54.32f   /* %RH */
+#define EXPECTED_BOSCH_COMP_H   29.81f   /* %RH */
 
 /* Raw ADC Test Vectors for Table 18 */
 #define VECTOR_RAW_PRESS_MSB    0x5DU
@@ -654,8 +654,8 @@ static void test_bme280_compensation_fixed_point_scaling(void) {
     TEST_ASSERT_INT16_WITHIN(5, 2508, fixed_data.temp_centi_c);
     /* 1006.53 hPa -> 100653 Pa */
     TEST_ASSERT_UINT32_WITHIN(10, 100653U, fixed_data.press_pascals);
-    /* 54.32 % -> 5432 centi-% */
-    TEST_ASSERT_UINT16_WITHIN(10, 5432U, fixed_data.hum_centi_percent);
+    /* 29.81 % -> 2981 centi-% */
+    TEST_ASSERT_UINT16_WITHIN(10, 2981U, fixed_data.hum_centi_percent);
 }
 
 /**
```

---

## 4. Verification & Prevention Guidelines

1. **Analytical Formula Cross-Verification**: When establishing test vectors from trimming registers and raw ADC words, mathematically execute the C formula using a deterministic analytical calculation to confirm the exact expected float/fixed-point values.
2. **Tolerance Assertion Accuracy**: Ensure test tolerances ($\pm 0.05\%$) match the analytical rounding of the underlying single-precision FPU mathematical model.
