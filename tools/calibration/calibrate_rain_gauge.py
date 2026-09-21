#!/usr/bin/env python3
"""
calibrate_rain_gauge.py
-----------------------
Sprint 7 Task S7-T3.2: Tipping-Bucket Rain Gauge Field Water Calibration Tool.
Calculates volumetric calibration factors, checks bucket seesaw symmetry,
computes dynamic flow compensation, and exports LoRaWAN downlink frames.

Author: Automated Firmware Verification Engine
Reference: WMO-No. 8 & S7-T3.2 Specification
"""

import math
import argparse
import struct
import json
import sys
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Tuple, Dict, Optional

# ==============================================================================
# 1. Physics & Meteorological Constants
# ==============================================================================
DEFAULT_FUNNEL_DIAMETER_MM = 200.0  # Standard agricultural collector (mm)
NOMINAL_TIP_DEPTH_MM = 0.20         # Nominal depth per tip (mm)
WATER_DENSITY_G_ML = 1.000          # Pure water density at 20°C (g/mL)

@dataclass
class RainCalibrationRecord:
    station_id: str
    funnel_diameter_mm: float
    funnel_area_cm2: float
    nominal_tip_volume_ml: float
    dispensed_volume_ml: float
    dispense_duration_min: float
    simulated_intensity_mmh: float
    tips_left: int
    tips_right: int
    total_tips: int
    expected_tips: float
    calibration_error_pct: float
    calibrated_k_factor_mm: float
    symmetry_status: str
    pass_verification: bool
    downlink_hex: str

class RainGaugeCalibrator:
    """Tipping-Bucket volumetric physics and calibration engine."""

    @staticmethod
    def calculate_funnel_area_cm2(diameter_mm: float) -> float:
        """Computes collector funnel aperture area in cm^2."""
        radius_cm = (diameter_mm / 10.0) / 2.0
        return round(math.pi * radius_cm * radius_cm, 4)

    @staticmethod
    def calculate_tip_volume_ml(diameter_mm: float, tip_depth_mm: float = 0.20) -> float:
        """Calculates theoretical water volume per tip in mL (cm^3)."""
        area_cm2 = RainGaugeCalibrator.calculate_funnel_area_cm2(diameter_mm)
        depth_cm = tip_depth_mm / 10.0
        return round(area_cm2 * depth_cm, 4)

    @staticmethod
    def calculate_simulated_intensity(volume_ml: float, diameter_mm: float, duration_min: float) -> float:
        """Calculates simulated rain intensity in mm/hr from dispensing rate."""
        if duration_min <= 0.0:
            raise ValueError("Dispense duration must be strictly positive")
        area_cm2 = RainGaugeCalibrator.calculate_funnel_area_cm2(diameter_mm)
        depth_mm = (volume_ml / area_cm2) * 10.0
        intensity_mmh = depth_mm * (60.0 / duration_min)
        return round(intensity_mmh, 2)

    @staticmethod
    def evaluate_calibration(volume_ml: float,
                             tips_left: int,
                             tips_right: int,
                             diameter_mm: float = DEFAULT_FUNNEL_DIAMETER_MM) -> Tuple[float, float, float, str, bool]:
        """Evaluates volumetric error, chamber symmetry, and calibrated multiplier."""
        total_tips = tips_left + tips_right
        if total_tips == 0:
            raise ValueError("Total recorded tips must be greater than zero")

        v_tip = RainGaugeCalibrator.calculate_tip_volume_ml(diameter_mm, NOMINAL_TIP_DEPTH_MM)
        expected_tips = volume_ml / v_tip
        error_pct = ((total_tips - expected_tips) / expected_tips) * 100.0
        k_calibrated = NOMINAL_TIP_DEPTH_MM * (expected_tips / total_tips)

        # Symmetry Check
        tip_diff = abs(tips_left - tips_right)
        if tip_diff <= 1:
            symmetry = "BALANCED (Delta <= 1 Tip)"
        elif tip_diff <= 3:
            symmetry = f"MINOR_ASYMMETRY (Delta = {tip_diff} Tips)"
        else:
            symmetry = f"ASYMMETRIC_FAULT (Delta = {tip_diff} Tips - Re-level & Adjust Stops)"

        passed = (abs(error_pct) <= 2.00) and (tip_diff <= 2)
        return round(expected_tips, 2), round(error_pct, 2), round(k_calibrated, 4), symmetry, passed

    @staticmethod
    def generate_lorawan_downlink(k_factor_mm: float) -> Tuple[bytes, str]:
        """Encodes LoRaWAN Downlink Command 0x05 (Set Rain Gauge K-Factor)."""
        k_factor_um = int(round(k_factor_mm * 1000.0))
        if k_factor_um < 150 or k_factor_um > 250:
            raise ValueError(f"K-Factor {k_factor_mm} mm/tip out of safety range [0.150, 0.250] mm")

        # Cmd 0x05, K_Factor in micrometers (uint16_t BE)
        payload = struct.pack(">B H", 0x05, k_factor_um)
        return payload, payload.hex().upper()

def run_verification_suite():
    """Runs automated verification assertions matching TC-RG-01..10."""
    print("\n[*] Running Verification Suite TC-RG-01..10:")

    # TC-RG-01: Funnel Catch Area (200mm)
    area = RainGaugeCalibrator.calculate_funnel_area_cm2(200.0)
    assert abs(area - 314.16) <= 0.05, f"TC-RG-01 Failed: expected ~314.16 cm^2, got {area}"
    print(f"  [PASS] TC-RG-01: Funnel Catch Area: {area} cm^2")

    # TC-RG-02: Theoretical Tip Volume (200mm, 0.20mm)
    v_tip = RainGaugeCalibrator.calculate_tip_volume_ml(200.0, 0.20)
    assert abs(v_tip - 6.2832) <= 0.001, f"TC-RG-02 Failed: expected 6.2832 mL, got {v_tip}"
    print(f"  [PASS] TC-RG-02: Theoretical Tip Volume: {v_tip} mL")

    # TC-RG-03: Expected Tips for 500 mL
    exp_tips = 500.0 / v_tip
    assert abs(exp_tips - 79.58) <= 0.10, f"TC-RG-03 Failed: expected ~79.58 tips, got {exp_tips}"
    print(f"  [PASS] TC-RG-03: Expected Tips (500 mL): {exp_tips:.2f} tips")

    # TC-RG-04: Calibration Error & Multiplier (40 left + 40 right = 80 tips)
    exp, err, k, sym, p = RainGaugeCalibrator.evaluate_calibration(500.0, 40, 40, 200.0)
    assert abs(err - 0.53) <= 0.05, f"TC-RG-04 Failed: expected error ~+0.53%, got {err}%"
    assert abs(k - 0.1989) <= 0.001, f"TC-RG-04 Failed: expected K ~0.1989 mm, got {k}"
    assert p is True, "TC-RG-04 Failed: expected pass=True"
    print(f"  [PASS] TC-RG-04: Calibration Error: {err:+.2f}%, K={k:.4f} mm/tip")

    # TC-RG-05: Under-tipping compensation (38 left + 38 right = 76 tips)
    exp5, err5, k5, sym5, p5 = RainGaugeCalibrator.evaluate_calibration(500.0, 38, 38, 200.0)
    assert abs(err5 - (-4.50)) <= 0.05, f"TC-RG-05 Failed: expected error ~-4.50%, got {err5}%"
    assert abs(k5 - 0.2094) <= 0.001, f"TC-RG-05 Failed: expected K ~0.2094 mm, got {k5}"
    print(f"  [PASS] TC-RG-05: Under-tipping Compensation: {err5:+.2f}%, K={k5:.4f} mm/tip")

    # TC-RG-06: Simulated Rainfall Rate Timing (500 mL, 25 mm/hr)
    duration_min = (500.0 / (area * 10.0 / 100.0)) * 60.0 / 25.0  # 500 / 31.4159 * 60 / 25
    rate = RainGaugeCalibrator.calculate_simulated_intensity(500.0, 200.0, 38.20)
    assert abs(rate - 25.0) <= 0.2, f"TC-RG-06 Failed: expected ~25 mm/hr, got {rate}"
    print(f"  [PASS] TC-RG-06: Simulated Rainfall Rate: {rate} mm/hr over 38.2 min")

    # TC-RG-07: Chamber Symmetry Balanced Gate
    exp7, err7, k7, sym7, p7 = RainGaugeCalibrator.evaluate_calibration(500.0, 40, 39, 200.0)
    assert "BALANCED" in sym7, f"TC-RG-07 Failed: expected BALANCED, got {sym7}"
    print("  [PASS] TC-RG-07: Chamber Symmetry (Delta = 1 tip): BALANCED")

    # TC-RG-08: Chamber Asymmetry Fault (43 left, 37 right -> Delta = 6)
    exp8, err8, k8, sym8, p8 = RainGaugeCalibrator.evaluate_calibration(500.0, 43, 37, 200.0)
    assert "ASYMMETRIC_FAULT" in sym8, f"TC-RG-08 Failed: expected ASYMMETRIC_FAULT, got {sym8}"
    assert p8 is False, "TC-RG-08 Failed: asymmetric fault should fail acceptance"
    print(f"  [PASS] TC-RG-08: Chamber Asymmetry (Delta = 6 tips): {sym8}")

    # TC-RG-09: LoRaWAN Downlink K-Factor Frame Parsing (K = 0.201 mm -> 201 um -> 0x00C9)
    payload, hex_str = RainGaugeCalibrator.generate_lorawan_downlink(0.201)
    assert hex_str == "0500C9", f"TC-RG-09 Failed: expected 0500C9, got {hex_str}"
    print(f"  [PASS] TC-RG-09: LoRaWAN Downlink Hex: 0x{hex_str} (Cmd 0x05, 201 um)")

    # TC-RG-10: Safety Boundary Clamping on Out-of-Range K-Factors
    try:
        RainGaugeCalibrator.generate_lorawan_downlink(0.100)
        assert False, "TC-RG-10 Failed: K < 0.150 not rejected"
    except ValueError:
        pass
    try:
        RainGaugeCalibrator.generate_lorawan_downlink(0.300)
        assert False, "TC-RG-10 Failed: K > 0.250 not rejected"
    except ValueError:
        pass
    print("  [PASS] TC-RG-10: Safety Boundary Clamping [0.150, 0.250] mm")

    print("\n[+] All Rain Gauge Verification Assertions Passed Successfully (100% Pass Rate)!\n")

# ==============================================================================
# 2. CLI Interface & Execution
# ==============================================================================
def main():
    if sys.platform == "win32":
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(description="Sprint 7 Task S7-T3.2: Tipping-Bucket Rain Gauge Calibration Tool.")
    parser.add_argument("--station-id", type=str, default="NODE-MUNNAR-RG01", help="Target Station ID.")
    parser.add_argument("--diameter", type=float, default=200.0, help="Funnel collector diameter in mm (default: 200.0).")
    parser.add_argument("--volume-ml", type=float, default=500.0, help="Total water volume dispensed in mL (default: 500.0).")
    parser.add_argument("--duration-min", type=float, default=19.1, help="Dispensing duration in minutes (default: 19.1).")
    parser.add_argument("--tips-left", type=int, default=40, help="Tips counted on left bucket chamber (default: 40).")
    parser.add_argument("--tips-right", type=int, default=40, help="Tips counted on right bucket chamber (default: 40).")
    parser.add_argument("--output-json", type=str, default="data/rain_gauge_calibration_report.json", help="Path to output JSON.")
    parser.add_argument("--verify", action="store_true", help="Run automated TC-RG-01..10 verification assertions.")
    args = parser.parse_args()

    print("=" * 78)
    print("[*] TIPPING-BUCKET RAIN GAUGE WATER CALIBRATION & VERIFICATION (S7-T3.2)")
    print(f"[*] Station ID: {args.station_id} | Funnel Diameter: {args.diameter} mm")
    print("=" * 78)

    if args.verify:
        run_verification_suite()
        return

    area_cm2 = RainGaugeCalibrator.calculate_funnel_area_cm2(args.diameter)
    v_tip_nominal = RainGaugeCalibrator.calculate_tip_volume_ml(args.diameter, NOMINAL_TIP_DEPTH_MM)
    intensity_mmh = RainGaugeCalibrator.calculate_simulated_intensity(args.volume_ml, args.diameter, args.duration_min)

    exp_tips, error_pct, k_factor, symmetry, passed = RainGaugeCalibrator.evaluate_calibration(
        args.volume_ml, args.tips_left, args.tips_right, args.diameter
    )

    payload_bytes, hex_str = RainGaugeCalibrator.generate_lorawan_downlink(k_factor)

    print("\n[+] Geometric & Flow Parameters:")
    print(f"    - Funnel Aperture Area             : {area_cm2:7.2f} cm^2")
    print(f"    - Theoretical Tip Volume (0.20mm)  : {v_tip_nominal:7.4f} mL ({v_tip_nominal:7.4f} g)")
    print(f"    - Dispensed Calibration Volume     : {args.volume_ml:7.1f} mL over {args.duration_min:4.1f} min")
    print(f"    - Simulated Rainfall Rate          : {intensity_mmh:7.2f} mm/hr")

    print("\n[+] Experimental Calibration Results:")
    print(f"    - Left Chamber Tips (N_left)       : {args.tips_left:4d} tips")
    print(f"    - Right Chamber Tips (N_right)     : {args.tips_right:4d} tips")
    print(f"    - Total Recorded Tips (N_actual)   : {args.tips_left + args.tips_right:4d} tips")
    print(f"    - Expected Theoretical Tips        : {exp_tips:7.2f} tips")
    print(f"    - Volumetric Error Percentage      : {error_pct:+7.2f} %")
    print(f"    - Chamber Symmetry Status          : {symmetry}")
    print(f"    - Calibrated Multiplier (K_gauge)  : {k_factor:7.4f} mm/tip ({int(k_factor*1000)} um/tip)")
    status_str = "[PASS]" if passed else "[FAIL - Adjust Stop Screws]"
    print(f"    - Acceptance Verification Gate     : {status_str}")

    print("\n[+] LoRaWAN Downlink Configuration Frame (FPort 10):")
    print(f"    - Binary Hex Stream                : 0x{hex_str}")
    print(f"    - Breakdown (3 Bytes)              : [Cmd: 0x05] [K_factor: {int(k_factor*1000)} um]")

    record = RainCalibrationRecord(
        station_id=args.station_id,
        funnel_diameter_mm=args.diameter,
        funnel_area_cm2=area_cm2,
        nominal_tip_volume_ml=v_tip_nominal,
        dispensed_volume_ml=args.volume_ml,
        dispense_duration_min=args.duration_min,
        simulated_intensity_mmh=intensity_mmh,
        tips_left=args.tips_left,
        tips_right=args.tips_right,
        total_tips=args.tips_left + args.tips_right,
        expected_tips=exp_tips,
        calibration_error_pct=error_pct,
        calibrated_k_factor_mm=k_factor,
        symmetry_status=symmetry,
        pass_verification=passed,
        downlink_hex=hex_str
    )

    out_path = Path(args.output_json)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(asdict(record), f, indent=2)
    print(f"\n[+] Exported rain gauge calibration report to: {out_path.resolve()}\n")

    if not passed:
        sys.exit(1)

if __name__ == "__main__":
    main()
