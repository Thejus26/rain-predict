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

from simulate_plantation_weather import EnvironmentalState, MicroclimateEngine


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
