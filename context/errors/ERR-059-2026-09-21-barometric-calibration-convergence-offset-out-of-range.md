# Error Report: ERR-059 - Excessive Residual Offset in test_tc_cal_10_multi_site_convergence Triggers CAL_STATUS_ERR_OUT_OF_RANGE

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Error ID** | `ERR-059` |
| **Date & Time** | 2026-09-21 09:27:17 +05:30 |
| **Commit SHA** | [`bfcacd1`](https://github.com/Thejus26/rain-predict/commit/bfcacd1d34a79a3e1d1752bf67bdf404dbbfb829) |
| **Component / Subsystem** | Algorithm Numerical Precision & Physical Vectors |
| **Sprint & Task** | Sprint 7 (`S7-T3.1` Barometric Altitude Calibration & Multi-Site Verification) |
| **Severity** | High (CI Unit Test Assertion Failure: `Expected 0 Was 2`) |
| **Impacted Files** | [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c)<br>[`tools/calibration/calibrate_station_elevation.py`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tools/calibration/calibrate_station_elevation.py) |

---

## 1. Description & Symptoms

During remote GitHub Actions CI execution of host unit test suite `test_barometric_calibration`, test case `test_tc_cal_10_multi_site_convergence` failed with the following diagnostic trace:

```text
/home/runner/work/rain-predict/rain-predict/tests/unit/test_barometric_calibration.c:321:test_tc_cal_10_multi_site_convergence:FAIL: Expected 0 Was 2
```

In [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c):
- Line 321 asserts `TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status);` where `CAL_STATUS_OK` is `0`.
- The returned status was `2`, which corresponds to `CAL_STATUS_ERR_OUT_OF_RANGE`.

---

## 2. Root Cause Analysis

In [`tests/unit/test_barometric_calibration.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c), `test_tc_cal_10_multi_site_convergence` defines test vector records across three estate mast sites:

```c
    const struct site_record sites[3] = {
        {843.20f, 18.5f, 1520.0f, 1008.40f},
        {778.50f, 15.0f, 2200.0f, 1012.35f},
        {940.00f, 26.0f,  650.0f, 1012.30f}
    };
```

During iteration $i = 1$ (Nilgiris Ridge site, $h = 2200.0\text{ m}, T = 15.0^\circ\text{C}, P_{\text{raw}} = 778.50\text{ hPa}$):
1. **Uncalibrated Sea-Level Calculation**:
   Calling `compute_sea_level_pressure(778.50f, 15.0f, 2200.0f, 0.0f, &p0_calc)` computes:
   $$P_{0,\text{calc}} \approx 1004.19\text{ hPa}$$
2. **Residual Trim Offset Calculation**:
   The test computes the required trim offset to match target reference $P_{0,\text{target}} = 1012.35\text{ hPa}$:
   $$\text{residual} = P_{0,\text{target}} - P_{0,\text{calc}} = 1012.35 - 1004.19 = +8.16\text{ hPa}$$
3. **Boundary Protection Violation**:
   Per the NVM specification and boundary protection rules in [`context/specs/s7-t3.1-barometric-altitude-calibration.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s7-t3.1-barometric-altitude-calibration.md), sensor trim offsets are strictly limited to $\pm 5.00\text{ hPa}$ (`-500` to `+500` centihPa in NVM `press_offset_chpa`).
   In [`compute_sea_level_pressure`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_barometric_calibration.c#L87-L89):
   ```c
   if (offset_hpa < -5.0f || offset_hpa > 5.0f) {
       return CAL_STATUS_ERR_OUT_OF_RANGE;
   }
   ```
   Because $+8.16\text{ hPa} > +5.00\text{ hPa}$, passing `residual` as the calibration offset triggers `CAL_STATUS_ERR_OUT_OF_RANGE` (`2`), failing the assertion `TEST_ASSERT_EQUAL_INT(CAL_STATUS_OK, status)`.

The root cause is an inconsistent physical test vector:
- If raw station pressure at $2200\text{ m}$ is $778.50\text{ hPa}$, the uncalibrated sea-level pressure is $1004.24\text{ hPa}$ (as verified in `TC-CAL-02`). Setting target $P_0 = 1012.35\text{ hPa}$ creates an unphysically large discrepancy ($8.16\text{ hPa}$), well beyond allowable barometric drift.
- Conversely, for a synoptic reference sea-level pressure of $1012.35\text{ hPa}$ at $h = 2200.0\text{ m}$ and $T = 15.0^\circ\text{C}$, the physical station pressure $P_{\text{raw}}$ is $784.80\text{ hPa}$, yielding a realistic residual $\le \pm 0.05\text{ hPa}$.

---

## 3. Resolution & Code Changes *(Implemented)*

### Fix Strategy

1. **Correct Site 1 Test Vector in `tests/unit/test_barometric_calibration.c`**:
   Updated Site 1 in `sites[3]` from `778.50f` to physical station pressure `784.80f` for reference target `1012.35f` ($P_{0,\text{calc}} \approx 1012.37\text{ hPa}$, residual $\approx -0.02\text{ hPa}$, well within $\pm 5.00\text{ hPa}$).
2. **Synchronize Python Tool `tools/calibration/calibrate_station_elevation.py`**:
   - Added explicit boundary enforcement `offset_hpa < -5.0 or offset_hpa > 5.0` in `BarometricCalibrator.calculate_sea_level_pressure` to ensure exact parity with C99 firmware validation.
   - Updated `test_sites` in `calibrate_station_elevation.py` to use `(784.80, 15.0, 2200.0, 1012.35)`.

### Code Changes

```diff
--- a/tests/unit/test_barometric_calibration.c
+++ b/tests/unit/test_barometric_calibration.c
@@ -304,7 +304,7 @@ static void test_tc_cal_10_multi_site_convergence(void)
 
     const struct site_record sites[3] = {
         {843.20f, 18.5f, 1520.0f, 1008.40f},
-        {778.50f, 15.0f, 2200.0f, 1012.35f},
+        {784.80f, 15.0f, 2200.0f, 1012.35f},
         {940.00f, 26.0f,  650.0f, 1012.30f}
     };

--- a/tools/calibration/calibrate_station_elevation.py
+++ b/tools/calibration/calibrate_station_elevation.py
@@ -51,6 +51,8 @@ class BarometricCalibrator:
         """Calculates Mean Sea Level Pressure (P0) using standard hypsometric formula."""
         if elevation_m < -100.0 or elevation_m > 3500.0:
             raise ValueError(f"Elevation {elevation_m}m out of physical bounds [-100, 3500]m")
+        if offset_hpa < -5.0 or offset_hpa > 5.0:
+            raise ValueError(f"Pressure offset {offset_hpa} hPa out of physical bounds [-5.0, 5.0] hPa")
         temp_k = temperature_c + 273.15
         lapse_elevation = STANDARD_LAPSE_RATE * elevation_m
         base = 1.0 - (lapse_elevation / (temp_k + lapse_elevation))
@@ -154,7 +156,7 @@ def run_verification_suite():
     # TC-CAL-10: Multi-Site Convergence
     test_sites = [
         (843.20, 18.5, 1520.0, 1008.40),
-        (778.50, 15.0, 2200.0, 1012.35),
+        (784.80, 15.0, 2200.0, 1012.35),
         (940.00, 26.0,  650.0, 1012.30),
         (849.07, 18.5, 1542.5, 1014.12)
     ]
```

---

## 4. Verification & Prevention Guidelines

1. **Sensor Trim Offset Bounds**:
   - When defining multi-site calibration convergence tests, verify that `|P0_target - P0_calc|` does not exceed the hardware and firmware architectural limit ($\pm 5.00\text{ hPa}$).
2. **Physical Vector Cross-Validation**:
   - Cross-check synthetic test vectors between Python tooling and C unit tests using the exact same hypsometric equation and physical boundary constraints.
