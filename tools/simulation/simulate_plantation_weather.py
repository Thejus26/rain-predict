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


class StormEventCoordinator:
    """Coordinates storm injection models, rainfall generation, and ground-truth lead time tracking."""

    def __init__(
        self,
        scenario: str = "pre_monsoon_convective",
        seed: Optional[int] = None,
    ):
        self.scenario = scenario
        self.rain_bucket_reservoir_mm = 0.0
        self.rng = random.Random(seed) if seed is not None else random

    def reset(self) -> None:
        """Resets bucket reservoir state."""
        self.rain_bucket_reservoir_mm = 0.0

    def process_rain_step(self, rain_rate_mmh: float, dt_hours: float) -> Tuple[int, float]:
        """
        Calculates accumulated rain and bucket tips during the discrete time step.
        Bucket calibration factor = 0.20 mm per tip.
        """
        step_rainfall_mm = rain_rate_mmh * dt_hours
        self.rain_bucket_reservoir_mm += step_rainfall_mm
        new_tips = int((self.rain_bucket_reservoir_mm + 1e-7) / 0.20)
        actual_tipped_mm = new_tips * 0.20
        self.rain_bucket_reservoir_mm -= actual_tipped_mm
        if self.rain_bucket_reservoir_mm < 1e-7:
            self.rain_bucket_reservoir_mm = 0.0
        return new_tips, round(actual_tipped_mm, 2)

    def apply_convective_storm(
        self,
        step_time_hours: float,
        temp_base: float,
        rh_base: float,
        p_base: float,
        p0_base: float,
        lux_base: float,
        dt_hours: float,
    ) -> Tuple[float, float, float, float, float, float, int, float, int, float]:
        """
        Applies pre-monsoon convective storm event centered at hour 14.0 (2:00 PM).
        Build-up: 11:30 - 14:00 (150 min lead time).
        Precipitation: 14:00 - 15:30 (90 min rain).
        Dissipation: 15:30 - 17:00 (90 min recovery).
        """
        storm_start_hour = 14.0
        storm_duration_hours = 1.5
        storm_buildup_hours = 2.5  # Build up starts at 11:30 AM
        storm_dissipation_hours = 1.5  # Dissipation until 17:00 (5:00 PM)

        temp = temp_base
        rh = rh_base
        p_station = p_base
        p0 = p0_base
        lux = lux_base
        rain_rate = 0.0
        is_raining = 0
        lead_time_min = 0.0

        time_to_storm = storm_start_hour - step_time_hours

        # 1. Pre-Storm Build-Up Window (11:30 to 14:00)
        if 0.0 < time_to_storm <= storm_buildup_hours:
            lead_time_min = time_to_storm * 60.0
            buildup_progress = (storm_buildup_hours - time_to_storm) / storm_buildup_hours

            # Barometric pressure drop: -3.2 hPa maximum drop over 2.5 hours
            p_drop = 3.2 * math.pow(buildup_progress, 1.4)
            p_station -= p_drop
            p0 -= p_drop

            # Relative humidity surge: rises toward 96%
            rh_surge = (96.0 - rh_base) * math.pow(buildup_progress, 1.2)
            rh = min(99.0, rh_base + rh_surge)

            # Solar irradiance collapse in last 45 minutes
            if time_to_storm <= 0.75:
                cloud_progress = (0.75 - time_to_storm) / 0.75
                lux_attenuation = 1.0 - 0.88 * math.pow(cloud_progress, 0.8)
                lux = max(1500.0, lux_base * lux_attenuation)

            # Evaporative downdraft cooling
            temp -= 3.5 * buildup_progress

        # 2. Active Rain Storm Window (14:00 to 15:30)
        elif 0.0 <= (step_time_hours - storm_start_hour) <= storm_duration_hours:
            rain_elapsed = step_time_hours - storm_start_hour
            # Bell-shaped rain rate profile peaking at 40 mm/hr (strictly positive during active storm)
            rain_progress = (rain_elapsed + dt_hours * 0.5) / (storm_duration_hours + dt_hours)
            rain_rate = max(1.0, 40.0 * math.sin(math.pi * rain_progress))
            is_raining = 1
            rh = 98.5
            temp = temp_base - 5.0
            p_station -= 3.5
            p0 -= 3.5
            lux = min(5000.0, lux_base * 0.08)

        # 3. Post-Storm Dissipation Window (15:30 to 17:00)
        elif storm_duration_hours < (step_time_hours - storm_start_hour) <= (
            storm_duration_hours + storm_dissipation_hours
        ):
            diss_progress = (
                step_time_hours - (storm_start_hour + storm_duration_hours)
            ) / storm_dissipation_hours
            p_recovery = 3.5 * (1.0 - diss_progress)
            p_station -= p_recovery
            p0 -= p_recovery
            temp -= 5.0 * (1.0 - diss_progress)
            rh = min(99.0, rh_base + (98.5 - rh_base) * (1.0 - diss_progress))
            lux = lux_base * (0.08 + 0.92 * diss_progress)

        # Process tipping-bucket pulses
        tips, actual_rain_tipped = self.process_rain_step(rain_rate, dt_hours)

        return (
            round(temp, 2),
            round(min(100.0, max(0.0, rh)), 2),
            round(p_station, 2),
            round(p0, 2),
            round(max(0.0, lux), 1),
            round(rain_rate, 2),
            tips,
            round(actual_rain_tipped, 2),
            is_raining,
            round(lead_time_min, 1),
        )

    def apply_monsoon_sustained(
        self,
        step_time_hours: float,
        temp_base: float,
        rh_base: float,
        p_base: float,
        p0_base: float,
        lux_base: float,
        dt_hours: float,
    ) -> Tuple[float, float, float, float, float, float, int, float, int, float]:
        """
        Applies sustained orographic monsoon rain profile.
        Continuous high humidity (95-100%), low solar Lux (<10000 Lux), depressed pressure (-6.0 hPa),
        and continuous rain rate (2.0 to 8.0 mm/hr).
        """
        p_station = p_base - 6.0
        p0 = p0_base - 6.0
        rh = min(99.5, max(95.0, rh_base + 35.0))
        temp = temp_base - 3.0
        lux = min(8000.0, lux_base * 0.10)

        # Low-to-moderate rain rate varying smoothly between 2.5 and 6.5 mm/hr
        rain_rate = 4.5 + 2.0 * math.sin(math.pi * step_time_hours / 12.0)
        is_raining = 1
        lead_time_min = 0.0

        tips, actual_rain_tipped = self.process_rain_step(rain_rate, dt_hours)

        return (
            round(temp, 2),
            round(min(100.0, max(0.0, rh)), 2),
            round(p_station, 2),
            round(p0, 2),
            round(max(0.0, lux), 1),
            round(rain_rate, 2),
            tips,
            round(actual_rain_tipped, 2),
            is_raining,
            round(lead_time_min, 1),
        )

    def apply_cloud_shadow_false_alarm(
        self,
        step_time_hours: float,
        temp_base: float,
        rh_base: float,
        p_base: float,
        p0_base: float,
        lux_base: float,
    ) -> Tuple[float, float, float, float, float, float, int, float, int, float]:
        """
        Applies non-rain fair-weather cloud shadow between 13:00 and 13:30.
        Drops solar Lux by 80%, but pressure and humidity remain unaffected.
        """
        lux = lux_base
        if 13.0 <= step_time_hours <= 13.5:
            # 80% solar reduction
            lux = lux_base * 0.20

        return (
            round(temp_base, 2),
            round(min(100.0, max(0.0, rh_base)), 2),
            round(p_base, 2),
            round(p0_base, 2),
            round(max(0.0, lux), 1),
            0.0,
            0,
            0.0,
            0,
            0.0,
        )

    def apply_orographic_fog_false_alarm(
        self,
        step_time_hours: float,
        temp_base: float,
        rh_base: float,
        p_base: float,
        p0_base: float,
        lux_base: float,
    ) -> Tuple[float, float, float, float, float, float, int, float, int, float]:
        """
        Applies morning valley fog between 06:00 and 09:00.
        High RH (>= 98%) and low Lux (< 5000 Lux), but barometric pressure continues normal morning rise.
        """
        temp = temp_base
        rh = rh_base
        lux = lux_base

        if 6.0 <= step_time_hours <= 9.0:
            rh = max(rh_base, 98.0)
            lux = min(lux_base, 4500.0)
            temp = temp_base - 1.0

        return (
            round(temp, 2),
            round(min(100.0, max(0.0, rh)), 2),
            round(p_base, 2),
            round(p0_base, 2),
            round(max(0.0, lux), 1),
            0.0,
            0,
            0.0,
            0,
            0.0,
        )

    def apply_fair_weather(
        self,
        step_time_hours: float,
        temp_base: float,
        rh_base: float,
        p_base: float,
        p0_base: float,
        lux_base: float,
    ) -> Tuple[float, float, float, float, float, float, int, float, int, float]:
        """Returns unaltered fair-weather physical baseline."""
        return (
            round(temp_base, 2),
            round(min(100.0, max(0.0, rh_base)), 2),
            round(p_base, 2),
            round(p0_base, 2),
            round(max(0.0, lux_base), 1),
            0.0,
            0,
            0.0,
            0,
            0.0,
        )

    def apply_scenario(
        self,
        scenario: str,
        step_time_hours: float,
        temp_base: float,
        rh_base: float,
        p_base: float,
        p0_base: float,
        lux_base: float,
        dt_hours: float,
        day_index: int = 0,
    ) -> Tuple[float, float, float, float, float, float, int, float, int, float]:
        """Routes time step to the appropriate scenario disturbance model."""
        target_scenario = scenario
        if scenario == "multi_day_storm_cycle":
            cycle = [
                "fair_weather",
                "pre_monsoon_convective",
                "false_alarm_cloud_shadow",
                "false_alarm_orographic_fog",
                "monsoon_sustained",
                "pre_monsoon_convective",
                "fair_weather",
            ]
            target_scenario = cycle[day_index % len(cycle)]

        if target_scenario == "pre_monsoon_convective":
            return self.apply_convective_storm(
                step_time_hours, temp_base, rh_base, p_base, p0_base, lux_base, dt_hours
            )
        elif target_scenario == "monsoon_sustained":
            return self.apply_monsoon_sustained(
                step_time_hours, temp_base, rh_base, p_base, p0_base, lux_base, dt_hours
            )
        elif target_scenario == "false_alarm_cloud_shadow":
            return self.apply_cloud_shadow_false_alarm(
                step_time_hours, temp_base, rh_base, p_base, p0_base, lux_base
            )
        elif target_scenario == "false_alarm_orographic_fog":
            return self.apply_orographic_fog_false_alarm(
                step_time_hours, temp_base, rh_base, p_base, p0_base, lux_base
            )
        else:
            return self.apply_fair_weather(
                step_time_hours, temp_base, rh_base, p_base, p0_base, lux_base
            )


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
        self.coordinator = StormEventCoordinator(
            scenario=scenario, seed=seed
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
        self.coordinator.reset()

        for step in range(self.total_steps):
            current_time = self.start_time + datetime.timedelta(minutes=step * self.interval_min)
            elapsed_hours = step * self.dt_hours
            day_index = int(elapsed_hours // 24.0)
            hour_of_day = (current_time.hour + current_time.minute / 60.0) % 24.0

            # 1. Base Diurnal Physical Profile (S1-T3.2)
            t_base, rh_base, p_base, p0_base, lux_base = self.diurnal_generator.generate_step(
                hour_of_day, add_noise=True
            )

            # 2. Convective Storm / Scenario Overlays (S1-T3.3)
            (
                temp_c,
                rh_pct,
                p_station,
                _p0_step,
                lux,
                rain_rate,
                tips,
                actual_rain_tipped,
                is_raining,
                lead_time_min,
            ) = self.coordinator.apply_scenario(
                scenario=self.scenario,
                step_time_hours=hour_of_day,
                temp_base=t_base,
                rh_base=rh_base,
                p_base=p_base,
                p0_base=p0_base,
                lux_base=lux_base,
                dt_hours=self.dt_hours,
                day_index=day_index,
            )

            self.total_rain_tips += tips
            self.accumulated_rain_mm += actual_rain_tipped

            # Calculate sea-level pressure using hypsometric formula
            p0 = self.calculate_barometric_reduction(p_station, temp_c)

            state = EnvironmentalState(
                timestamp=current_time.isoformat(),
                step_index=step,
                elapsed_hours=round(elapsed_hours, 2),
                temp_c=round(temp_c, 2),
                humidity_pct=round(min(100.0, max(0.0, rh_pct)), 2),
                pressure_hpa=round(p_station, 2),
                sea_level_pressure_hpa=p0,
                solar_lux=round(max(0.0, lux), 1),
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
        exporter = DatasetExporter(self.states, self.elevation_m, self.interval_min)
        exporter.export_csv(output_path)

    def export_json(self, output_path: str) -> None:
        """Exports simulation records to JSON array."""
        exporter = DatasetExporter(self.states, self.elevation_m, self.interval_min)
        exporter.export_json(output_path)

    def export_c_header(self, output_path: str) -> None:
        """Exports static C99 arrays and metadata macros for Unity host unit tests."""
        exporter = DatasetExporter(self.states, self.elevation_m, self.interval_min)
        exporter.export_c_header(output_path)

    def get_exporter(self) -> "DatasetExporter":
        """Returns a DatasetExporter bound to current simulation states."""
        return DatasetExporter(self.states, self.elevation_m, self.interval_min)


class DatasetExporter:
    """Handles CSV, JSON, and C Header generation along with dataset integrity validation and summary reporting."""

    def __init__(
        self,
        states: List[EnvironmentalState],
        elevation_m: float = 1500.0,
        interval_min: int = 15,
    ):
        self.states = states
        self.elevation_m = elevation_m
        self.interval_min = interval_min

    def export_csv(self, file_path: str) -> None:
        """Exports simulation records to an RFC 4180 compliant CSV file."""
        if not self.states:
            return
        out_file = Path(file_path)
        out_file.parent.mkdir(parents=True, exist_ok=True)
        fieldnames = list(asdict(self.states[0]).keys())

        with open(out_file, mode="w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for s in self.states:
                writer.writerow(asdict(s))

    def export_json(self, file_path: str) -> None:
        """Exports simulation records as a formatted JSON telemetry array."""
        if not self.states:
            return
        out_file = Path(file_path)
        out_file.parent.mkdir(parents=True, exist_ok=True)
        data = [asdict(s) for s in self.states]

        with open(out_file, mode="w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

    def export_c_header(self, file_path: str) -> None:
        """Exports static C99 arrays and metadata macros for Unity host unit tests."""
        if not self.states:
            return
        out_file = Path(file_path)
        out_file.parent.mkdir(parents=True, exist_ok=True)
        n = len(self.states)

        lines = [
            "/**",
            " * @file    simulated_weather_vectors.h",
            " * @brief   Auto-generated synthetic weather test vectors for host unit tests.",
            " * @note    Generated by simulate_plantation_weather.py. Do not edit manually.",
            " */\n",
            "#ifndef SIMULATED_WEATHER_VECTORS_H",
            "#define SIMULATED_WEATHER_VECTORS_H\n",
            "#include <stdint.h>\n",
            f"#define SIM_TOTAL_SAMPLES           {n}",
            f"#define SIM_INTERVAL_MINUTES        {self.interval_min}",
            f"#define SIM_STATION_ELEVATION_M     {self.elevation_m:.1f}f\n",
        ]

        def format_float_array(name: str, values: List[float]) -> str:
            val_strs = [f"{v:.2f}f" for v in values]
            body = ",\n    ".join([", ".join(val_strs[i : i + 8]) for i in range(0, len(val_strs), 8)])
            return f"static const float {name}[SIM_TOTAL_SAMPLES] = {{\n    {body}\n}};\n"

        def format_uint8_array(name: str, values: List[int]) -> str:
            val_strs = [str(v) for v in values]
            body = ",\n    ".join([", ".join(val_strs[i : i + 16]) for i in range(0, len(val_strs), 16)])
            return f"static const uint8_t {name}[SIM_TOTAL_SAMPLES] = {{\n    {body}\n}};\n"

        lines.append(format_float_array("SIM_TEMP_C", [s.temp_c for s in self.states]))
        lines.append(format_float_array("SIM_HUMIDITY_PCT", [s.humidity_pct for s in self.states]))
        lines.append(format_float_array("SIM_PRESSURE_HPA", [s.pressure_hpa for s in self.states]))
        lines.append(format_float_array("SIM_SEA_LEVEL_P0_HPA", [s.sea_level_pressure_hpa for s in self.states]))
        lines.append(format_float_array("SIM_SOLAR_LUX", [s.solar_lux for s in self.states]))
        lines.append(format_uint8_array("SIM_IS_RAINING", [s.is_raining for s in self.states]))
        lines.append(format_float_array("SIM_LEAD_TIME_MIN", [s.ground_truth_lead_time_min for s in self.states]))

        lines.append("#endif /* SIMULATED_WEATHER_VECTORS_H */\n")

        with open(out_file, mode="w", encoding="utf-8") as f:
            f.write("\n".join(lines))

    def validate_integrity(self) -> Tuple[bool, List[str]]:
        """
        Validates the dataset against meteorological constraints and schema invariants.
        Returns (is_valid, list_of_errors).
        """
        errors: List[str] = []
        if not self.states:
            errors.append("Dataset is empty.")
            return False, errors

        prev_step = -1
        prev_elapsed = -1.0
        prev_tips = -1
        prev_accum_rain = -1.0

        for i, s in enumerate(self.states):
            if s.step_index != i:
                errors.append(f"Step {i}: step_index {s.step_index} != {i}")
            if s.step_index <= prev_step and i > 0:
                errors.append(f"Step {i}: step_index not strictly monotonic ({s.step_index} <= {prev_step})")
            if s.elapsed_hours < prev_elapsed:
                errors.append(f"Step {i}: elapsed_hours decreased ({s.elapsed_hours} < {prev_elapsed})")

            # Physical ranges
            if not (-20.0 <= s.temp_c <= 60.0):
                errors.append(f"Step {i}: temperature {s.temp_c} °C out of physical bounds [-20, 60]")
            if not (0.0 <= s.humidity_pct <= 100.0):
                errors.append(f"Step {i}: humidity {s.humidity_pct} % out of bounds [0, 100]")
            if not (300.0 <= s.pressure_hpa <= 1100.0):
                errors.append(f"Step {i}: station pressure {s.pressure_hpa} hPa out of bounds [300, 1100]")
            if not (800.0 <= s.sea_level_pressure_hpa <= 1100.0):
                errors.append(f"Step {i}: sea-level pressure {s.sea_level_pressure_hpa} hPa out of bounds [800, 1100]")
            if s.solar_lux < 0.0:
                errors.append(f"Step {i}: negative solar lux {s.solar_lux}")

            # Rain and tipping bucket invariants
            if s.is_raining not in (0, 1):
                errors.append(f"Step {i}: is_raining ({s.is_raining}) must be 0 or 1")

            if s.is_raining == 1:
                if s.rain_rate_mmh <= 0.0:
                    errors.append(f"Step {i}: is_raining == 1 but rain_rate_mmh is {s.rain_rate_mmh}")
                if s.ground_truth_lead_time_min != 0.0:
                    errors.append(f"Step {i}: is_raining == 1 but lead_time is {s.ground_truth_lead_time_min}")

            if s.rain_gauge_tip_count < prev_tips:
                errors.append(f"Step {i}: rain_gauge_tip_count decreased ({s.rain_gauge_tip_count} < {prev_tips})")
            if s.accumulated_rain_mm < prev_accum_rain:
                errors.append(f"Step {i}: accumulated_rain_mm decreased ({s.accumulated_rain_mm} < {prev_accum_rain})")

            # Tip conservation: accumulated_rain_mm should equal tip_count * 0.20 (within rounding precision)
            expected_rain = round(s.rain_gauge_tip_count * 0.20, 2)
            if abs(s.accumulated_rain_mm - expected_rain) > 0.05:
                errors.append(
                    f"Step {i}: rainfall conservation mismatch (accumulated {s.accumulated_rain_mm} mm != tips {s.rain_gauge_tip_count} * 0.20 = {expected_rain} mm)"
                )

            prev_step = s.step_index
            prev_elapsed = s.elapsed_hours
            prev_tips = s.rain_gauge_tip_count
            prev_accum_rain = s.accumulated_rain_mm

        return len(errors) == 0, errors

    def count_storm_events(self) -> int:
        """Counts the number of distinct contiguous rain periods in the dataset."""
        events = 0
        in_event = False
        for s in self.states:
            if s.is_raining == 1 and not in_event:
                events += 1
                in_event = True
            elif s.is_raining == 0 and in_event:
                in_event = False
        return events

    def get_summary_text(
        self,
        scenario: str,
        seed: Optional[int] = None,
        export_files: Optional[Dict[str, str]] = None,
    ) -> str:
        """Formats and returns the ANSI terminal summary report string."""
        n = len(self.states)
        if n == 0:
            return "No simulation data available."

        temps = [s.temp_c for s in self.states]
        rhs = [s.humidity_pct for s in self.states]
        pressures = [s.pressure_hpa for s in self.states]
        slps = [s.sea_level_pressure_hpa for s in self.states]
        luxs = [s.solar_lux for s in self.states]
        max_rain_rate = max(s.rain_rate_mmh for s in self.states)
        total_rain = self.states[-1].accumulated_rain_mm
        total_tips = self.states[-1].rain_gauge_tip_count
        max_lead_time = max(s.ground_truth_lead_time_min for s in self.states)
        storm_events = self.count_storm_events()
        duration_days = (n * self.interval_min) / (24 * 60)
        seed_str = f"{seed} (Deterministic)" if seed is not None else "Random / Unseeded"

        delta_p_max = max(pressures) - min(pressures)

        lines = [
            "=============================================================================",
            " Tea Plantation Synthetic Microclimate Simulation Summary",
            "=============================================================================",
            f" Scenario Profile         : {scenario}",
            f" Duration & Step Size     : {duration_days:.0f} days ({n} records @ {self.interval_min}-min interval)"
            if duration_days.is_integer()
            else f" Duration & Step Size     : {duration_days:.1f} days ({n} records @ {self.interval_min}-min interval)",
            f" Station Elevation        : {self.elevation_m:.1f} m ASL",
            f" Random Seed              : {seed_str}",
            "-----------------------------------------------------------------------------",
            f" Temperature Range        : {min(temps):.2f} C (min) to {max(temps):.2f} C (max) | Mean: {sum(temps)/n:.2f} C",
            f" Relative Humidity Range  : {min(rhs):.2f} % (min) to {max(rhs):.2f} % (max) | Mean: {sum(rhs)/n:.2f} %",
            f" Station Barometric P     : {min(pressures):.2f} hPa (min) to {max(pressures):.2f} hPa (max) | Delta P_max: {delta_p_max:.2f} hPa",
            f" Sea-Level Barometric P0  : {min(slps):.2f} hPa (min) to {max(slps):.2f} hPa (max)",
            f" Peak Solar Irradiance    : {max(luxs):,.1f} Lux",
            f" Total Precipitation      : {total_rain:.2f} mm ({total_tips} bucket tips @ 0.2 mm/tip)",
            f" Peak Rain Rate           : {max_rain_rate:.2f} mm/hr",
            f" Storm Events Simulated   : {storm_events} {'events' if storm_events != 1 else 'event'}",
            f" Max Prediction Lead Time : {max_lead_time:.1f} minutes",
            "-----------------------------------------------------------------------------",
        ]

        if export_files:
            lines.append(" Export Files Generated   :")
            for tag, file_info in export_files.items():
                lines.append(f"   - {tag:<5}: {file_info}")
            lines.append("=============================================================================")
        else:
            lines.append("=============================================================================")

        return "\n".join(lines)

    def print_summary(
        self,
        scenario: str,
        seed: Optional[int] = None,
        export_files: Optional[Dict[str, str]] = None,
    ) -> None:
        """Prints the formatted simulation summary to stdout."""
        print(self.get_summary_text(scenario, seed, export_files))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Tea Plantation Microclimate Weather Simulator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--duration-days", type=int, default=7, help="Simulation duration in days")
    parser.add_argument(
        "--interval-min",
        type=int,
        default=15,
        choices=[1, 5, 10, 15],
        help="Temporal resolution step in minutes",
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
            "false_alarm_orographic_fog",
            "multi_day_storm_cycle",
        ],
        help="Weather simulation scenario profile",
    )
    parser.add_argument("--seed", type=int, default=42, help="Random number generator seed")
    parser.add_argument("--output", type=str, default="data/simulated_weather.csv", help="Output file path")
    parser.add_argument(
        "--format",
        type=str,
        default="csv",
        choices=["csv", "json", "c-header", "both", "all"],
        help="Export format",
    )
    parser.add_argument(
        "--export-c-header",
        type=str,
        default=None,
        help="Optional path to export C99 array header (e.g. tests/unit/simulated_weather_vectors.h)",
    )
    parser.add_argument("--verbose", action="store_true", help="Print summary statistics and details")
    parser.add_argument("--summary", action="store_true", help="Print formatted simulation summary report")
    parser.add_argument(
        "--check-integrity", action="store_true", help="Validate dataset integrity against meteorological constraints"
    )
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
    exporter = engine.get_exporter()

    # Integrity validation check if requested
    if args.check_integrity:
        valid, errors = exporter.validate_integrity()
        if not valid:
            print("[SIM] ERROR: Dataset integrity validation failed:", file=sys.stderr)
            for err in errors:
                print(f"  - {err}", file=sys.stderr)
            return 1
        print(f"[SIM] Dataset integrity check PASSED ({len(states)} records validated).")

    export_files: Dict[str, str] = {}
    base_stem = Path(args.output).with_suffix("")

    want_csv = args.format in ["csv", "both", "all"]
    want_json = args.format in ["json", "both", "all"]
    want_c_header = (args.format in ["c-header", "all"]) or bool(args.export_c_header)

    if want_csv:
        if args.output.endswith(".csv"):
            csv_path = args.output
        elif args.output.endswith(".json") or args.output.endswith(".h"):
            csv_path = str(base_stem) + ".csv"
        else:
            csv_path = args.output if not Path(args.output).suffix else str(base_stem) + ".csv"
        exporter.export_csv(csv_path)
        p = Path(csv_path)
        sz_kb = p.stat().st_size / 1024.0 if p.exists() else 0.0
        export_files["CSV"] = f"{csv_path} ({len(states)} rows, {sz_kb:.1f} KB)"
        print(f"[SIM] Successfully exported {len(states)} records to CSV: {csv_path}")

    if want_json:
        if args.output.endswith(".json"):
            json_path = args.output
        elif args.output.endswith(".csv") or args.output.endswith(".h"):
            json_path = str(base_stem) + ".json"
        else:
            json_path = f"{args.output}.json" if not Path(args.output).suffix else str(base_stem) + ".json"
        exporter.export_json(json_path)
        p = Path(json_path)
        sz_kb = p.stat().st_size / 1024.0 if p.exists() else 0.0
        export_files["JSON"] = f"{json_path} ({len(states)} items, {sz_kb:.1f} KB)"
        print(f"[SIM] Successfully exported {len(states)} records to JSON: {json_path}")

    if want_c_header:
        c_path = args.export_c_header if args.export_c_header else str(base_stem) + ".h"
        exporter.export_c_header(c_path)
        p = Path(c_path)
        sz_kb = p.stat().st_size / 1024.0 if p.exists() else 0.0
        export_files["C .h"] = f"{c_path} (C99 Array Headers, {sz_kb:.1f} KB)"
        print(f"[SIM] Successfully exported C header vectors to: {c_path}")

    if args.verbose or args.summary:
        print()
        exporter.print_summary(scenario=args.scenario, seed=args.seed, export_files=export_files)

    return 0


if __name__ == "__main__":
    sys.exit(main())
