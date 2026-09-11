#!/usr/bin/env python3
"""
test_simulator.py
------------------
Unit tests for Python Weather Simulator Core Engine (S1-T3.1).
Validates MicroclimateEngine state generation, hypsometric reduction,
temporal resolution stepping, deterministic seeds, and CSV/JSON serialization.
"""

import csv
import json
import math
import pathlib
import sys
import tempfile
import unittest

# Ensure tools/simulation is in Python module search path
REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "simulation"))

from simulate_plantation_weather import (
    DiurnalProfileGenerator,
    EnvironmentalState,
    MicroclimateEngine,
)


class TestMicroclimateEngine(unittest.TestCase):
    """Test suite for MicroclimateEngine core coordinator."""

    def setUp(self):
        self.engine = MicroclimateEngine(
            duration_days=1,
            interval_min=15,
            elevation_m=1500.0,
            scenario="pre_monsoon_convective",
            seed=42,
        )

    def test_step_count_calculation(self):
        """Verify step counts match duration and temporal resolution."""
        # 1 day @ 15 min = 96 steps
        self.assertEqual(self.engine.total_steps, 96)

        engine_1min = MicroclimateEngine(duration_days=1, interval_min=1)
        self.assertEqual(engine_1min.total_steps, 1440)

        engine_7day_15min = MicroclimateEngine(duration_days=7, interval_min=15)
        self.assertEqual(engine_7day_15min.total_steps, 672)

    def test_hypsometric_barometric_reduction(self):
        """Verify hypsometric sea-level pressure conversion formula against WMO vectors."""
        # TC-HYP-01: Sea-Level Baseline (0m ASL, 1013.25 hPa @ 15.0 C)
        engine_0m = MicroclimateEngine(elevation_m=0.0)
        self.assertAlmostEqual(
            engine_0m.calculate_barometric_reduction(1013.25, 15.0), 1013.25, places=2
        )

        # TC-HYP-02: Munnar Valley Station (1000m ASL, 898.70 hPa @ 22.0 C) -> 1009.01 hPa
        engine_1000m = MicroclimateEngine(elevation_m=1000.0)
        self.assertAlmostEqual(
            engine_1000m.calculate_barometric_reduction(898.70, 22.0), 1009.01, delta=0.05
        )

        # TC-HYP-03: Coonoor Mid-Slope (1500m ASL, 845.20 hPa @ 18.5 C) -> 1007.61 hPa
        engine_1500m = MicroclimateEngine(elevation_m=1500.0)
        self.assertAlmostEqual(
            engine_1500m.calculate_barometric_reduction(845.20, 18.5), 1007.61, delta=0.05
        )

        # TC-HYP-04: Ooty Ridge Peak (2000m ASL, 785.40 hPa @ 14.0 C) -> 996.48 hPa
        engine_2000m = MicroclimateEngine(elevation_m=2000.0)
        self.assertAlmostEqual(
            engine_2000m.calculate_barometric_reduction(785.40, 14.0), 996.48, delta=0.05
        )

        # TC-HYP-05: Kolukkumalai High Mast (2160m ASL, 767.10 hPa @ 12.0 C) -> 993.77 hPa
        engine_2160m = MicroclimateEngine(elevation_m=2160.0)
        self.assertAlmostEqual(
            engine_2160m.calculate_barometric_reduction(767.10, 12.0), 993.77, delta=0.05
        )

    def test_simulation_run_state_structure(self):
        """Verify all fields in EnvironmentalState records are populated within valid physical bounds."""
        states = self.engine.run()
        self.assertEqual(len(states), 96)

        for i, s in enumerate(states):
            self.assertEqual(s.step_index, i)
            self.assertIsInstance(s.timestamp, str)
            self.assertGreaterEqual(s.humidity_pct, 10.0)
            self.assertLessEqual(s.humidity_pct, 100.0)
            self.assertGreaterEqual(s.solar_lux, 0.0)
            self.assertGreaterEqual(s.rain_gauge_tip_count, 0)
            self.assertGreaterEqual(s.accumulated_rain_mm, 0.0)
            self.assertIn(s.is_raining, (0, 1))
            self.assertEqual(s.scenario_tag, "pre_monsoon_convective")

    def test_deterministic_seed_reproducibility(self):
        """Verify identical seed generates bit-for-bit identical state sequences."""
        engine_a = MicroclimateEngine(duration_days=3, interval_min=15, seed=12345)
        engine_b = MicroclimateEngine(duration_days=3, interval_min=15, seed=12345)

        states_a = engine_a.run()
        states_b = engine_b.run()

        self.assertEqual(len(states_a), len(states_b))
        for sa, sb in zip(states_a, states_b):
            self.assertEqual(sa, sb)

    def test_csv_export(self):
        """Verify CSV export formatting and header presence."""
        self.engine.run()
        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = pathlib.Path(tmpdir) / "sim.csv"
            self.engine.export_csv(str(csv_path))

            self.assertTrue(csv_path.exists())
            with open(csv_path, "r", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                rows = list(reader)

            self.assertEqual(len(rows), 96)
            self.assertIn("timestamp", rows[0])
            self.assertIn("temp_c", rows[0])
            self.assertIn("humidity_pct", rows[0])
            self.assertIn("pressure_hpa", rows[0])
            self.assertIn("sea_level_pressure_hpa", rows[0])
            self.assertIn("solar_lux", rows[0])

    def test_json_export(self):
        """Verify JSON export formatting and schema compliance."""
        self.engine.run()
        with tempfile.TemporaryDirectory() as tmpdir:
            json_path = pathlib.Path(tmpdir) / "sim.json"
            self.engine.export_json(str(json_path))

            self.assertTrue(json_path.exists())
            with open(json_path, "r", encoding="utf-8") as f:
                data = json.load(f)

            self.assertIsInstance(data, list)
            self.assertEqual(len(data), 96)
            self.assertEqual(data[0]["step_index"], 0)
            self.assertIn("sea_level_pressure_hpa", data[0])


class TestDiurnalProfileGenerator(unittest.TestCase):
    """Test suite for DiurnalProfileGenerator physical baseline models (S1-T3.2)."""

    def setUp(self):
        self.generator = DiurnalProfileGenerator(elevation_m=1500.0, seed=42)

    @staticmethod
    def _pearson_correlation(x: list, y: list) -> float:
        n = len(x)
        mean_x = sum(x) / n
        mean_y = sum(y) / n
        cov = sum((xi - mean_x) * (yi - mean_y) for xi, yi in zip(x, y))
        var_x = sum((xi - mean_x) ** 2 for xi in x)
        var_y = sum((yi - mean_y) ** 2 for yi in y)
        if var_x <= 0 or var_y <= 0:
            return 0.0
        return cov / math.sqrt(var_x * var_y)

    def test_tc_s1_t3_2_01_solar_lux_profile(self):
        """TC-S1-T3.2-01: Solar Lux Profile Verification (daylight window, noon peak)."""
        # Night hours: 0 Lux before 06:00 and after 18:00
        for h in [0.0, 1.0, 4.0, 5.0, 19.0, 22.0, 23.5]:
            lux = self.generator.compute_solar_lux(h, add_noise=False)
            self.assertEqual(lux, 0.0, f"Expected 0 Lux at hour {h}, got {lux}")

        # Sunrise at 06:00 and sunset at 18:00
        self.assertAlmostEqual(
            self.generator.compute_solar_lux(6.0, add_noise=False), 0.0, places=1
        )
        self.assertAlmostEqual(
            self.generator.compute_solar_lux(18.0, add_noise=False), 0.0, places=1
        )

        # Solar noon peak at 12:00 must exceed 100,000 Lux (at 1500m elevation factor = 1.10 -> 121,000 Lux)
        noon_lux = self.generator.compute_solar_lux(12.0, add_noise=False)
        self.assertGreater(noon_lux, 100000.0)
        self.assertAlmostEqual(noon_lux, 121000.0, delta=100.0)

    def test_tc_s1_t3_2_02_temperature_range_and_peak_time(self):
        """TC-S1-T3.2-02: Temperature Range & Asymmetric Peak Time (min 05:00-06:00, max 13:30-14:30)."""
        hours = [h * 0.25 for h in range(96)]  # 15-minute steps across 24h
        temps = [self.generator.compute_temperature(h, add_noise=False) for h in hours]

        min_temp = min(temps)
        max_temp = max(temps)
        min_hour = hours[temps.index(min_temp)]
        max_hour = hours[temps.index(max_temp)]

        # Verify minimum occurs between 05:00 and 06:00 (exact: 05:30)
        self.assertGreaterEqual(min_hour, 5.0)
        self.assertLessEqual(min_hour, 6.0)

        # Verify maximum occurs between 13:30 and 14:30 (exact: 14:00)
        self.assertGreaterEqual(max_hour, 13.5)
        self.assertLessEqual(max_hour, 14.5)

        # Verify diurnal temperature swing amplitude (nominal 1500m: 16.5 C +/- 6.5 C -> 10.0 to 23.0 C)
        self.assertAlmostEqual(min_temp, 10.0, delta=0.1)
        self.assertAlmostEqual(max_temp, 23.0, delta=0.1)
        self.assertAlmostEqual(max_temp - min_temp, 13.0, delta=0.1)

    def test_tc_s1_t3_2_03_psychrometric_humidity_anticorrelation(self):
        """TC-S1-T3.2-03: Psychrometric Humidity Anti-Correlation (r < -0.90, max RH at min T, min RH at max T)."""
        hours = [h * 0.25 for h in range(96)]
        temps = [self.generator.compute_temperature(h, add_noise=False) for h in hours]
        rhs = [self.generator.compute_relative_humidity(t, add_noise=False) for t in temps]

        # At minimum temperature (05:30, T=10.0 C), RH reaches daily max (> 85%)
        t_min = self.generator.compute_temperature(5.5, add_noise=False)
        rh_at_t_min = self.generator.compute_relative_humidity(t_min, add_noise=False)
        self.assertGreater(rh_at_t_min, 85.0)

        # At maximum temperature (14:00, T=23.0 C), RH reaches daily min (< 65%)
        t_max = self.generator.compute_temperature(14.0, add_noise=False)
        rh_at_t_max = self.generator.compute_relative_humidity(t_max, add_noise=False)
        self.assertLess(rh_at_t_max, 65.0)

        # Verify strong inverse anti-correlation r < -0.90
        r = self._pearson_correlation(temps, rhs)
        self.assertLess(r, -0.90, f"Expected Pearson r < -0.90, got {r:.4f}")

    def test_tc_s1_t3_2_04_atmospheric_solar_tide_periodicity(self):
        """TC-S1-T3.2-04: Atmospheric Solar Tide Periodicity (12h cycle, dual peaks ~10:00/~22:00, troughs ~04:00/~16:00)."""
        hours = [h * 0.25 for h in range(96)]
        stn_pressures = [
            self.generator.compute_barometric_pressure(h, add_noise=False)[0] for h in hours
        ]

        # Check peak times (10:00 and 22:00)
        p_10am = self.generator.compute_barometric_pressure(10.0, add_noise=False)[0]
        p_10pm = self.generator.compute_barometric_pressure(22.0, add_noise=False)[0]
        p_04am = self.generator.compute_barometric_pressure(4.0, add_noise=False)[0]
        p_04pm = self.generator.compute_barometric_pressure(16.0, add_noise=False)[0]

        # Peaks should equal nominal + 1.50 hPa
        self.assertAlmostEqual(p_10am, self.generator.nominal_station_p + 1.50, delta=0.05)
        self.assertAlmostEqual(p_10pm, self.generator.nominal_station_p + 1.50, delta=0.05)

        # Troughs should equal nominal - 1.50 hPa
        self.assertAlmostEqual(p_04am, self.generator.nominal_station_p - 1.50, delta=0.05)
        self.assertAlmostEqual(p_04pm, self.generator.nominal_station_p - 1.50, delta=0.05)

        # Total peak-to-trough swing is 3.0 hPa (amplitude 1.5 +/- 0.3 hPa)
        swing = max(stn_pressures) - min(stn_pressures)
        self.assertAlmostEqual(swing, 3.0, delta=0.1)

    def test_tc_s1_t3_2_05_elevation_lapse_scaling(self):
        """TC-S1-T3.2-05: Elevation Lapse Scaling (500m vs 2000m: delta T ~9.75 C, station P ~955 hPa vs ~795 hPa)."""
        gen_500m = DiurnalProfileGenerator(elevation_m=500.0, seed=42)
        gen_2000m = DiurnalProfileGenerator(elevation_m=2000.0, seed=42)

        # Mean temperature lapse rate: 0.0065 C/m * 1500m = 9.75 C
        delta_temp = gen_500m.base_temp_mean - gen_2000m.base_temp_mean
        self.assertAlmostEqual(delta_temp, 9.75, delta=0.01)

        # Station pressure reduction (~955 hPa at 500m to ~795-798 hPa at 2000m)
        self.assertAlmostEqual(gen_500m.nominal_station_p, 956.42, delta=1.5)
        self.assertAlmostEqual(gen_2000m.nominal_station_p, 798.12, delta=1.5)

    def test_tc_s1_t3_2_06_bounds_and_clamping(self):
        """TC-S1-T3.2-06: Zero Negative / Out-of-Bounds Values over 100-day simulation."""
        gen = DiurnalProfileGenerator(elevation_m=1500.0, seed=123)

        # Simulate 100 days @ 15-min intervals = 9600 steps with noise
        for step in range(9600):
            hour = (step * 0.25) % 24.0
            temp_c, rh_pct, station_p, sea_level_p, solar_lux = gen.generate_step(
                hour, add_noise=True
            )

            self.assertGreaterEqual(rh_pct, 0.0, f"RH < 0 at step {step}: {rh_pct}")
            self.assertLessEqual(rh_pct, 100.0, f"RH > 100 at step {step}: {rh_pct}")
            self.assertGreaterEqual(solar_lux, 0.0, f"Lux < 0 at step {step}: {solar_lux}")
            self.assertGreater(station_p, 500.0, f"Pressure abnormal at step {step}: {station_p}")
            self.assertGreater(sea_level_p, 800.0, f"SLP abnormal at step {step}: {sea_level_p}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
