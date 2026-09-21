#!/usr/bin/env python3
"""
calibrate_station_elevation.py
------------------------------
Sprint 7 Task S7-T3.1: On-Site Barometric Altitude Calibration & Verification Tool.
Calculates hypsometric sea-level pressure, derives station elevation from reference
meteorological stations, formats LoRaWAN FPort 10 downlinks, and validates WMO accuracy.
"""

import math
import argparse
import struct
import json
import sys
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Tuple, Optional, Dict

# ==============================================================================
# 1. Atmospheric Constants & Physics
# ==============================================================================
STANDARD_LAPSE_RATE = 0.0065   # K/m (6.5 K/km)
HYPSOMETRIC_EXPONENT = 5.257  # (g * M) / (R * Γ)
GRAVITY = 9.80665              # m/s^2
MOLAR_MASS_AIR = 0.0289644     # kg/mol
GAS_CONSTANT = 8.31432         # J/(mol*K)

@dataclass
class CalibrationRecord:
    station_id: str
    raw_pressure_hpa: float
    ambient_temp_c: float
    reference_p0_hpa: float
    surveyed_elevation_m: float
    derived_elevation_m: float
    calibrated_p0_hpa: float
    error_delta_hpa: float
    regime_id: int
    downlink_hex: str
    passed: bool

class BarometricCalibrator:
    """Core meteorological calculation and calibration engine."""

    @staticmethod
    def calculate_sea_level_pressure(pressure_station_hpa: float,
                                     temperature_c: float,
                                     elevation_m: float,
                                     offset_hpa: float = 0.0) -> float:
        """Calculates Mean Sea Level Pressure (P0) using standard hypsometric formula."""
        if elevation_m < -100.0 or elevation_m > 3500.0:
            raise ValueError(f"Elevation {elevation_m}m out of physical bounds [-100, 3500]m")
        temp_k = temperature_c + 273.15
        lapse_elevation = STANDARD_LAPSE_RATE * elevation_m
        base = 1.0 - (lapse_elevation / (temp_k + lapse_elevation))
        if base <= 0.0:
            raise ValueError("Invalid atmospheric base ratio in hypsometric calculation")
        p0 = pressure_station_hpa * math.pow(base, -HYPSOMETRIC_EXPONENT)
        return round(p0 + offset_hpa, 2)

    @staticmethod
    def derive_station_elevation(pressure_station_hpa: float,
                                 reference_p0_hpa: float,
                                 temperature_c: float) -> float:
        """Derives effective station elevation AMSL from known reference sea-level pressure."""
        if pressure_station_hpa >= reference_p0_hpa:
            raise ValueError("Station pressure must be strictly less than reference sea-level pressure")
        temp_k = temperature_c + 273.15
        pressure_ratio = reference_p0_hpa / pressure_station_hpa
        elevation = (temp_k / STANDARD_LAPSE_RATE) * (math.pow(pressure_ratio, 1.0 / HYPSOMETRIC_EXPONENT) - 1.0)
        return round(elevation, 1)

    @staticmethod
    def generate_lorawan_downlink_frame(elevation_m: float,
                                        offset_hpa: float,
                                        regime_id: int) -> Tuple[bytes, str]:
        """Encodes 6-byte LoRaWAN Downlink Configuration Frame (FPort 10)."""
        elevation_dm = int(round(elevation_m * 10.0))
        offset_chpa = int(round(offset_hpa * 100.0))
        
        if elevation_dm < 0 or elevation_dm > 35000:
            raise ValueError(f"Elevation {elevation_m}m exceeds NVM uint16 range")
        if offset_chpa < -500 or offset_chpa > 500:
            raise ValueError(f"Pressure offset {offset_hpa}hPa exceeds allowed ±5.00 hPa limits")
        if regime_id not in [1, 2, 3]:
            raise ValueError(f"Invalid microclimate regime ID {regime_id} (must be 1, 2, or 3)")

        # Byte 0: Cmd 0x02, Byte 1-2: Elev_dm (u16 BE), Byte 3-4: Offset_chpa (i16 BE), Byte 5: Regime (u8)
        payload = struct.pack(">B H h B", 0x02, elevation_dm, offset_chpa, regime_id)
        return payload, payload.hex().upper()

def run_verification_suite():
    """Runs automated verification assertions matching TC-CAL-01..10."""
    print("\n[*] Running Verification Suite TC-CAL-01..10:")

    # TC-CAL-01: Munnar (1500m)
    p0_1 = BarometricCalibrator.calculate_sea_level_pressure(845.20, 20.0, 1500.0, 0.0)
    assert abs(p0_1 - 1003.83) <= 0.10, f"TC-CAL-01 Failed: expected 1003.83, got {p0_1}"
    print(f"  [PASS] TC-CAL-01: Munnar (1500m) Reduction: {p0_1} hPa")

    # TC-CAL-02: Nilgiris (2200m)
    p0_2 = BarometricCalibrator.calculate_sea_level_pressure(778.50, 15.0, 2200.0, 0.0)
    assert abs(p0_2 - 1004.24) <= 0.10, f"TC-CAL-02 Failed: expected 1004.24, got {p0_2}"
    print(f"  [PASS] TC-CAL-02: Nilgiris (2200m) Reduction: {p0_2} hPa")

    # TC-CAL-03: Assam (120m)
    p0_3 = BarometricCalibrator.calculate_sea_level_pressure(998.40, 28.0, 120.0, 0.0)
    assert abs(p0_3 - 1012.07) <= 0.15, f"TC-CAL-03 Failed: expected 1012.07, got {p0_3}"
    print(f"  [PASS] TC-CAL-03: Assam (120m) Reduction: {p0_3} hPa")

    # TC-CAL-04: Elevation Inversion
    h_inv = BarometricCalibrator.derive_station_elevation(845.00, 1013.25, 20.0)
    assert abs(h_inv - 1585.0) <= 1.0, f"TC-CAL-04 Failed: expected 1585.0m, got {h_inv}"
    print(f"  [PASS] TC-CAL-04: Elevation Inversion: {h_inv}m")

    # TC-CAL-05: Trim Offset Addition
    p0_trim = BarometricCalibrator.calculate_sea_level_pressure(845.00, 20.0, 1585.0, 0.25)
    assert abs(p0_trim - 1013.50) <= 0.05, f"TC-CAL-05 Failed: expected 1013.50 hPa, got {p0_trim}"
    print(f"  [PASS] TC-CAL-05: Trim Offset Addition: {p0_trim} hPa")

    # TC-CAL-06: Boundary Clamping Elevation
    try:
        BarometricCalibrator.calculate_sea_level_pressure(850.0, 20.0, -200.0, 0.0)
        assert False, "TC-CAL-06 Failed: negative elevation not rejected"
    except ValueError:
        pass
    try:
        BarometricCalibrator.calculate_sea_level_pressure(850.0, 20.0, 4000.0, 0.0)
        assert False, "TC-CAL-06 Failed: excessive elevation not rejected"
    except ValueError:
        pass
    print("  [PASS] TC-CAL-06: Elevation Boundary Clamping")

    # TC-CAL-07: Boundary Clamping Offset
    try:
        BarometricCalibrator.generate_lorawan_downlink_frame(1500.0, 6.0, 1)
        assert False, "TC-CAL-07 Failed: offset > 5.0 not rejected"
    except ValueError:
        pass
    try:
        BarometricCalibrator.generate_lorawan_downlink_frame(1500.0, -6.0, 1)
        assert False, "TC-CAL-07 Failed: offset < -5.0 not rejected"
    except ValueError:
        pass
    print("  [PASS] TC-CAL-07: Offset Boundary Clamping")

    # TC-CAL-08 & TC-CAL-09: LoRaWAN Downlink Encoding
    payload, hex_str = BarometricCalibrator.generate_lorawan_downlink_frame(1542.5, 0.15, 1)
    assert hex_str == "023C41000F01", f"TC-CAL-09 Failed: expected 023C41000F01, got {hex_str}"
    print(f"  [PASS] TC-CAL-08 & 09: LoRaWAN Downlink Hex: 0x{hex_str}")

    # TC-CAL-10: Multi-Site Convergence
    test_sites = [
        (843.20, 18.5, 1520.0, 1008.40),
        (778.50, 15.0, 2200.0, 1012.35),
        (940.00, 26.0,  650.0, 1012.30),
        (849.07, 18.5, 1542.5, 1014.12)
    ]
    for p_raw, temp, h_surv, p0_target in test_sites:
        p0_calc = BarometricCalibrator.calculate_sea_level_pressure(p_raw, temp, h_surv, 0.0)
        res = round(p0_target - p0_calc, 2)
        p0_calibrated = BarometricCalibrator.calculate_sea_level_pressure(p_raw, temp, h_surv, res)
        err = abs(p0_calibrated - p0_target)
        assert err <= 0.02, f"Convergence failed: err={err} > 0.02"
    print("  [PASS] TC-CAL-10: Multi-Site Convergence")
    print("\n[+] All Verification Assertions Passed Successfully (100% Pass Rate)!\n")

# ==============================================================================
# 2. CLI Interface & Execution
# ==============================================================================
def main():
    if sys.platform == "win32":
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(description="Sprint 7 Task S7-T3.1: On-Site Barometric Calibration Tool.")
    parser.add_argument("--station-id", type=str, default="NODE-MUNNAR-01", help="Target Station Identifier.")
    parser.add_argument("--raw-press", type=float, default=849.07, help="Raw station pressure at mast (hPa).")
    parser.add_argument("--temp", type=float, default=18.50, help="Ambient temperature in radiation shield (degC).")
    parser.add_argument("--ref-p0", type=float, default=1014.12, help="Reference MSL pressure from synoptic baseline (hPa).")
    parser.add_argument("--surveyed-alt", type=float, default=1542.5, help="Surveyed RTK GPS elevation in meters AMSL (optional).")
    parser.add_argument("--regime", type=int, default=1, choices=[1, 2, 3], help="1=High Ridge (>1200m), 2=Slope (600-1200m), 3=Valley (<600m).")
    parser.add_argument("--output-json", type=str, default="data/calibration_verification_report.json", help="Path to output JSON report.")
    parser.add_argument("--verify", action="store_true", help="Run automated TC-CAL-01..10 verification assertions.")
    args = parser.parse_args()

    print("=" * 78)
    print("[*] TEA PLANTATION ON-SITE BAROMETRIC CALIBRATION & VERIFICATION (S7-T3.1)")
    print(f"[*] Station ID: {args.station_id} | Microclimate Regime: {args.regime}")
    print("=" * 78)

    if args.verify:
        run_verification_suite()
        return

    # 1. Derive effective elevation from synoptic reference
    derived_alt = BarometricCalibrator.derive_station_elevation(args.raw_press, args.ref_p0, args.temp)
    target_elevation = args.surveyed_alt if args.surveyed_alt is not None else derived_alt

    # 2. Calculate uncompensated P0 and residual offset
    p0_uncomp = BarometricCalibrator.calculate_sea_level_pressure(args.raw_press, args.temp, target_elevation, 0.0)
    residual_offset = round(args.ref_p0 - p0_uncomp, 2)
    
    # Clamp offset within +/- 5.0 hPa
    if abs(residual_offset) > 5.0:
        print(f"[!] WARNING: Large residual offset ({residual_offset:+.2f} hPa). Clamping to physical limit [-5.0, +5.0] hPa.")
        residual_offset = max(-5.0, min(5.0, residual_offset))

    # 3. Compute final calibrated P0
    calibrated_p0 = BarometricCalibrator.calculate_sea_level_pressure(args.raw_press, args.temp, target_elevation, residual_offset)
    error_delta = abs(calibrated_p0 - args.ref_p0)
    passed = error_delta <= 0.50

    # 4. Generate Downlink Hex Frame
    payload_bytes, hex_str = BarometricCalibrator.generate_lorawan_downlink_frame(target_elevation, residual_offset, args.regime)

    print("\n[+] Atmospheric Inputs:")
    print(f"    - Raw Station Pressure (P_station) : {args.raw_press:7.2f} hPa")
    print(f"    - Radiation Shield Temperature (T) : {args.temp:7.2f} degC")
    print(f"    - Synoptic Reference Baseline (P0) : {args.ref_p0:7.2f} hPa")
    if args.surveyed_alt is not None:
        print(f"    - RTK Surveyed GPS Elevation (h)   : {args.surveyed_alt:7.1f} m AMSL")
    print(f"    - Barometrically Derived Elevation : {derived_alt:7.1f} m AMSL")

    print("\n[+] Calibration Results:")
    print(f"    - Effective Station Elevation (h)  : {target_elevation:7.1f} m AMSL")
    print(f"    - Trim Calibration Offset (dP)     : {residual_offset:+7.2f} hPa")
    print(f"    - Resulting Calibrated P0          : {calibrated_p0:7.2f} hPa")
    print(f"    - Absolute Discrepancy (|dError|)  : {error_delta:7.2f} hPa")
    status_str = "[PASS]" if passed else "[FAIL]"
    print(f"    - WMO Compliance Status (<= 0.5hPa): {status_str}")

    print("\n[+] LoRaWAN Downlink Configuration Frame (FPort 10):")
    print(f"    - Binary Hex Stream                : 0x{hex_str}")
    print(f"    - Breakdown (6 Bytes)              : [Cmd: 0x02] [Elev: {int(target_elevation*10)} dm] [Offset: {int(residual_offset*100)} chPa] [Regime: {args.regime}]")

    # Save verification report
    record = CalibrationRecord(
        station_id=args.station_id,
        raw_pressure_hpa=args.raw_press,
        ambient_temp_c=args.temp,
        reference_p0_hpa=args.ref_p0,
        surveyed_elevation_m=args.surveyed_alt if args.surveyed_alt else derived_alt,
        derived_elevation_m=derived_alt,
        calibrated_p0_hpa=calibrated_p0,
        error_delta_hpa=round(error_delta, 3),
        regime_id=args.regime,
        downlink_hex=hex_str,
        passed=passed
    )

    out_path = Path(args.output_json)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(asdict(record), f, indent=2)
    print(f"\n[+] Exported calibration verification report to: {out_path.resolve()}\n")

    if not passed:
        sys.exit(1)

if __name__ == "__main__":
    main()
