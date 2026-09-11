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


class DiurnalProfileGenerator:
    """Encapsulates meteorological formulas for fair-weather diurnal physical baselines."""

    def __init__(
        self,
        elevation_m: float = 1500.0,
        base_sea_level_p: float = 1013.25,
        seed: Optional[int] = None,
    ):
        self.elevation_m = elevation_m
        self.base_sea_level_p = base_sea_level_p
        self.rng = random.Random(seed) if seed is not None else random

        # Standard elevation lapse adjustment
        self.elevation_lapse = 0.0065 * (elevation_m - 500.0)
        self.base_temp_mean = 23.0 - self.elevation_lapse
        self.base_temp_amp = 6.5

        # Atmospheric tide parameters (12-hour period, peaks at 10:00 and 22:00)
        self.tide_amp_hpa = 1.50

        # Calculate nominal station barometric pressure using hypsometric formula
        t_mean_k = self.base_temp_mean + 273.15 + (0.0065 * elevation_m / 2.0)
        self.nominal_station_p = base_sea_level_p * math.pow(
            1.0 - (0.0065 * elevation_m) / t_mean_k, 5.257
        )

        # Base dew point at 1500m (dew point ~15.0 C at 500m gives realistic RH swings)
        self.nominal_dew_point_c = max(5.0, 15.0 - self.elevation_lapse)
        self.e_baseline = 6.112 * math.exp(
            (17.67 * self.nominal_dew_point_c) / (self.nominal_dew_point_c + 243.5)
        )

    def compute_solar_lux(self, hour_of_day: float, add_noise: bool = True) -> float:
        """Calculates clear-sky solar irradiance in Lux (6:00 to 18:00 daylight window)."""
        if 6.0 <= hour_of_day <= 18.0:
            # Insolation elevation factor (+2% per 300m above sea level)
            elevation_factor = 1.0 + 0.02 * (self.elevation_m / 300.0)
            max_lux = 110000.0 * elevation_factor

            solar_angle = math.pi * (hour_of_day - 6.0) / 12.0
            lux = max_lux * math.pow(math.sin(solar_angle), 1.35)
            # Add small atmospheric turbulence fluctuation
            noise = self.rng.gauss(0.0, 150.0) if add_noise else 0.0
            return max(0.0, lux + noise)
        return 0.0

    def compute_temperature(self, hour_of_day: float, add_noise: bool = True) -> float:
        """Calculates ambient temperature in Celsius with peak at 14:00 and pre-dawn minimum at 05:30."""
        # Asymmetric diurnal curve: warming from 05:30 to 14:00 (8.5h), cooling from 14:00 to 05:30 (15.5h)
        if 5.5 <= hour_of_day <= 14.0:
            theta = math.pi * (hour_of_day - 5.5) / 8.5
            t_amb = self.base_temp_mean - self.base_temp_amp * math.cos(theta)
        else:
            elapsed = (hour_of_day - 14.0) if hour_of_day >= 14.0 else (hour_of_day + 10.0)
            theta = math.pi * elapsed / 15.5
            t_amb = self.base_temp_mean + self.base_temp_amp * math.cos(theta)

        noise = self.rng.gauss(0.0, 0.10) if add_noise else 0.0
        return t_amb + noise

    def compute_relative_humidity(self, temp_c: float, add_noise: bool = True) -> float:
        """Calculates psychrometric relative humidity from saturation vapor pressure."""
        # Magnus formula for saturation vapor pressure
        es = 6.112 * math.exp((17.67 * temp_c) / (temp_c + 243.5))
        rh = (self.e_baseline / es) * 100.0
        noise = self.rng.gauss(0.0, 0.40) if add_noise else 0.0
        return min(100.0, max(30.0, rh + noise))

    def compute_barometric_pressure(
        self, hour_of_day: float, add_noise: bool = True
    ) -> Tuple[float, float]:
        """Calculates station pressure and sea-level pressure with semi-diurnal solar tide."""
        # Semi-diurnal atmospheric tide (12h cycle, peaks at 10:00 and 22:00, troughs at 04:00 and 16:00)
        tide_angle = math.pi * (hour_of_day - 10.0) / 6.0
        p_tide = self.tide_amp_hpa * math.cos(tide_angle)

        noise_stn = self.rng.gauss(0.0, 0.06) if add_noise else 0.0
        noise_slp = self.rng.gauss(0.0, 0.06) if add_noise else 0.0

        station_p = self.nominal_station_p + p_tide + noise_stn
        sea_level_p = self.base_sea_level_p + p_tide + noise_slp

        return round(station_p, 2), round(sea_level_p, 2)

    def generate_step(
        self, hour_of_day: float, add_noise: bool = True
    ) -> Tuple[float, float, float, float, float]:
        """Returns tuple of (temp_c, rh_pct, station_p, sea_level_p, solar_lux)."""
        temp_c = round(self.compute_temperature(hour_of_day, add_noise=add_noise), 2)
        rh_pct = round(self.compute_relative_humidity(temp_c, add_noise=add_noise), 2)
        station_p, sea_level_p = self.compute_barometric_pressure(
            hour_of_day, add_noise=add_noise
        )
        solar_lux = round(self.compute_solar_lux(hour_of_day, add_noise=add_noise), 1)

        return temp_c, rh_pct, station_p, sea_level_p, solar_lux


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

        self.diurnal_generator = DiurnalProfileGenerator(
            elevation_m=elevation_m, seed=seed
        )

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

            # 1. Base Diurnal Physical Profile (S1-T3.2)
            t_base, rh_base, p_base, p0_base, lux_base = self.diurnal_generator.generate_step(
                hour_of_day, add_noise=True
            )

            # 2. Convective Storm Overlays (S1-T3.3 Integration Point)
            rain_rate = 0.0
            is_raining = 0
            lead_time_min = 0.0

            # Calculate sea-level pressure using hypsometric formula
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
