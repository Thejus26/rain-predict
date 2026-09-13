# Error Report: ERR-012 - Unrealistic Hypsometric Test Vectors Causing Out-of-Range Errors

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-012` |
| **Date & Time** | `2026-09-12 11:28:18 +0530` |
| **Commit SHA** | [`6b8cdde`](https://github.com/Thejus26/rain-predict/commit/6b8cdde2e46c85eb9d241e6e37496899c4cefc18) |
| **Sprint / Task** | Sprint 2 (S2-T2.3 / S2-T2.4 Hypsometric Barometric Reduction Tests) |
| **Severity** | Medium (Test Assertion Failure / Validation Range Guard) |
| **Impacted Files** | [`tests/unit/test_dew_point.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tests/unit/test_dew_point.c) |

---

## 1. Description & Symptoms

When running `test_hypsometric_roundtrip()`, reducing barometric pressure at high altitude ($2000\,\text{m}$) returned error status `STATUS_ERR_OUT_OF_RANGE` instead of `STATUS_OK`.

---

## 2. Root Cause Analysis

The unit test input passed standard sea-level pressure ($1013.25\,\text{hPa}$) as the *station pressure* measured at $2000\,\text{m}$ elevation. Applying the hypsometric sea-level reduction formula on $1013.25\,\text{hPa}$ at $2000\,\text{m}$ yielded an estimated sea-level pressure $P_0 \approx 1300\,\text{hPa}$. This tripped the firmware's defensive atmospheric validity clamp ($[800, 1100]\,\text{hPa}$), returning `STATUS_ERR_OUT_OF_RANGE`.

---

## 3. Resolution & Code Changes

Updated the test vectors to use physically plausible mountain station pressures:
- Station pressure at $1500\,\text{m}$ (e.g. Munnar / Coonoor): $840.0\,\text{hPa} \rightarrow P_0 \approx 1005.6\,\text{hPa}$
- Station pressure at $2000\,\text{m}$ (e.g. Ooty / Kolukkumalai): $795.0\,\text{hPa} \rightarrow P_0 \approx 1012.3\,\text{hPa}$

```diff
-    float p_station = 1013.25f;
+    float p_station = 795.0f; /* Realistic measured pressure at 2000m */
     float altitude_m = 2000.0f;
```

---

## 4. Verification & Prevention Guidelines

- Synthetic test vectors for physical algorithms must conform to real-world meteorological atmospheric lapse rates.
- Defensive boundary checks ($[800, 1100]\,\text{hPa}$) must be tested with both valid boundary values and explicitly invalid out-of-range values.
