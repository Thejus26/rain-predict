#!/usr/bin/env python3
"""
simulate_plantation_weather.py
-------------------------------
Synthetic Microclimate Scenario Generator for Tea Plantation Rain Prediction.
Simulates diurnal cycles, convective storms, and barometric drops for firmware validation.
"""

import argparse
import csv
import datetime
import json
import math
import os
import random
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


@dataclass
class EnvironmentalState:
    timestamp: str
    step_index: int
    elapsed_hours: float
    temp_c: float
    humidity_pct: float
    pressure_hpa: float
    sea_level_pressure_hpa: float
    solar_lux: float
    rain_rate_mmh: float
    rain_gauge_tip_count: int
    accumulated_rain_mm: float
    is_raining: int
    ground_truth_lead_time_min: float
    scenario_tag: str


class MicroclimateEngine:
    """Core simulation coordinator managing time-stepping and state history."""

    def __init__(
        self,
        duration_days: int = 7,
        interval_min: int = 15,
        elevation_m: float = 1500.0,
        scenario: str = "pre_monsoon_convective",
        seed: Optional[int] = 42,
    ):
        self.duration_days = duration_days
        self.interval_min = interval_min
        self.elevation_m = elevation_m
        self.scenario = scenario
        self.seed = seed

        if seed is not None:
            random.seed(seed)

        self.total_steps = int((duration_days * 24 * 60) / interval_min)
        self.dt_hours = interval_min / 60.0
        self.start_time = datetime.datetime(2026, 9, 9, 0, 0, 0, tzinfo=datetime.timezone.utc)
        self.states: List[EnvironmentalState] = []
        self.accumulated_rain_mm = 0.0
        self.total_rain_tips = 0

    def calculate_barometric_reduction(self, p_station: float, temp_c: float) -> float:
        """Converts station pressure to sea-level equivalent using hypsometric formula."""
        if self.elevation_m <= 0.0:
            return round(p_station, 2)
        h = min(5000.0, max(0.0, self.elevation_m))
        lapse_h = 0.0065 * h
        t_mean_kelvin = max(200.0, temp_c + 273.15 + (lapse_h / 2.0))
        base = 1.0 - (lapse_h / t_mean_kelvin)
        p0 = p_station * math.pow(base, -5.257)
        return round(p0, 2)

    def run(self) -> List[EnvironmentalState]:
        """Executes simulation loop over all time steps."""
        self.states.clear()
        self.accumulated_rain_mm = 0.0
        self.total_rain_tips = 0

        for step in range(self.total_steps):
            current_time = self.start_time + datetime.timedelta(minutes=step * self.interval_min)
            elapsed_hours = step * self.dt_hours
            hour_of_day = (current_time.hour + current_time.minute / 60.0) % 24.0

            # 1. Base Diurnal Physical Profile (S1-T3.2 Integration Point)
            # Default placeholder: diurnal baseline sinusoidal curves
            t_base = 22.0 - 6.0 * math.cos(2.0 * math.pi * (hour_of_day - 4.0) / 24.0)
            rh_base = 75.0 + 20.0 * math.cos(2.0 * math.pi * (hour_of_day - 4.0) / 24.0)
            p_base = 845.0 - (self.elevation_m - 1500.0) * 0.10 + 1.5 * math.sin(2.0 * math.pi * hour_of_day / 12.0)

            # Solar irradiance calculation (Daylight peak at 12:00, 0 at night)
            if 6.0 <= hour_of_day <= 18.0:
                solar_factor = math.sin(math.pi * (hour_of_day - 6.0) / 12.0)
                lux_base = 100000.0 * math.pow(solar_factor, 1.5)
            else:
                lux_base = 0.0

            # 2. Convective Storm Overlays (S1-T3.3 Integration Point)
            rain_rate = 0.0
            is_raining = 0
            lead_time_min = 0.0

            # Calculate sea-level pressure
            p0 = self.calculate_barometric_reduction(p_base, t_base)

            state = EnvironmentalState(
                timestamp=current_time.isoformat(),
                step_index=step,
                elapsed_hours=round(elapsed_hours, 2),
                temp_c=round(t_base, 2),
                humidity_pct=round(min(100.0, max(10.0, rh_base)), 2),
                pressure_hpa=round(p_base, 2),
                sea_level_pressure_hpa=p0,
                solar_lux=round(max(0.0, lux_base), 1),
                rain_rate_mmh=round(rain_rate, 2),
                rain_gauge_tip_count=self.total_rain_tips,
                accumulated_rain_mm=round(self.accumulated_rain_mm, 2),
                is_raining=is_raining,
                ground_truth_lead_time_min=round(lead_time_min, 1),
                scenario_tag=self.scenario,
            )
            self.states.append(state)

        return self.states

    def export_csv(self, output_path: str) -> None:
        """Exports simulation records to CSV."""
        if not self.states:
            return

        out_file = Path(output_path)
        out_file.parent.mkdir(parents=True, exist_ok=True)

        fieldnames = list(asdict(self.states[0]).keys())
        with open(out_file, mode="w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for s in self.states:
                writer.writerow(asdict(s))

    def export_json(self, output_path: str) -> None:
        """Exports simulation records to JSON array."""
        if not self.states:
            return

        out_file = Path(output_path)
        out_file.parent.mkdir(parents=True, exist_ok=True)

        data = [asdict(s) for s in self.states]
        with open(out_file, mode="w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Tea Plantation Microclimate Weather Simulator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--duration-days", type=int, default=7, help="Simulation duration in days")
    parser.add_argument(
        "--interval-min", type=int, default=15, choices=[1, 5, 10, 15], help="Temporal resolution step in minutes"
    )
    parser.add_argument("--elevation", type=float, default=1500.0, help="Plantation elevation in meters ASL")
    parser.add_argument(
        "--scenario",
        type=str,
        default="pre_monsoon_convective",
        choices=[
            "fair_weather",
            "pre_monsoon_convective",
            "monsoon_sustained",
            "false_alarm_cloud_shadow",
            "multi_day_storm_cycle",
        ],
        help="Weather simulation scenario profile",
    )
    parser.add_argument("--seed", type=int, default=42, help="Random number generator seed")
    parser.add_argument("--output", type=str, default="data/simulated_weather.csv", help="Output file path")
    parser.add_argument("--format", type=str, default="csv", choices=["csv", "json", "both"], help="Export format")
    parser.add_argument("--verbose", action="store_true", help="Print summary statistics")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    engine = MicroclimateEngine(
        duration_days=args.duration_days,
        interval_min=args.interval_min,
        elevation_m=args.elevation,
        scenario=args.scenario,
        seed=args.seed,
    )

    states = engine.run()

    if args.format in ["csv", "both"]:
        if args.output.endswith(".json"):
            csv_path = args.output[:-5] + ".csv"
        elif args.output.endswith(".csv"):
            csv_path = args.output
        else:
            csv_path = f"{args.output}.csv"
        engine.export_csv(csv_path)
        print(f"[SIM] Successfully exported {len(states)} records to CSV: {csv_path}")

    if args.format in ["json", "both"]:
        if args.output.endswith(".csv"):
            json_path = args.output[:-4] + ".json"
        elif args.output.endswith(".json"):
            json_path = args.output
        else:
            json_path = f"{args.output}.json"
        engine.export_json(json_path)
        print(f"[SIM] Successfully exported {len(states)} records to JSON: {json_path}")

    if args.verbose:
        print(f"\n--- Simulation Summary ---")
        print(f"Total Duration : {args.duration_days} days ({len(states)} samples @ {args.interval_min}m)")
        print(f"Elevation      : {args.elevation} m ASL")
        print(f"Scenario       : {args.scenario}")
        print(f"Random Seed    : {args.seed}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
