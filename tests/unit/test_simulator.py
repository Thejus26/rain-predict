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
    StormEventCoordinator,
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


class TestStormEventCoordinator(unittest.TestCase):
    """Test suite for StormEventCoordinator convective models and false-alarm controls (S1-T3.3)."""

    def setUp(self):
        self.coordinator = StormEventCoordinator(scenario="pre_monsoon_convective", seed=42)
        self.diurnal_gen = DiurnalProfileGenerator(elevation_m=1500.0, seed=42)

    def test_tc_s1_t3_3_01_convective_storm_pressure_drop_rate(self):
        """TC-S1-T3.3-01: Convective Storm Pressure Drop Rate (Delta P >= 2.5 hPa / 3h before rain onset)."""
        # Rain onset is at 14:00 (14.0h). Build-up starts at 11:30 (11.5h).
        # We test the pressure drop across the pre-storm build-up window (11:00/11:30 to 14:00).
        t_base_11, rh_base_11, p_base_11, p0_base_11, lux_base_11 = (
            self.diurnal_gen.generate_step(11.0, add_noise=False)
        )
        res_11 = self.coordinator.apply_convective_storm(
            11.0, t_base_11, rh_base_11, p_base_11, p0_base_11, lux_base_11, dt_hours=0.25
        )
        p_11am = res_11[2]

        t_base_14, rh_base_14, p_base_14, p0_base_14, lux_base_14 = (
            self.diurnal_gen.generate_step(14.0, add_noise=False)
        )
        res_14 = self.coordinator.apply_convective_storm(
            14.0, t_base_14, rh_base_14, p_base_14, p0_base_14, lux_base_14, dt_hours=0.25
        )
        p_14pm = res_14[2]

        # Total drop over 3 hours preceding storm must be >= 2.5 hPa
        delta_p_3h = p_11am - p_14pm
        self.assertGreaterEqual(
            delta_p_3h, 2.5, f"Expected 3h pressure drop >= 2.5 hPa, got {delta_p_3h:.2f} hPa"
        )

        # Storm-induced perturbation alone at 14:00 must be >= 3.0 hPa
        storm_perturbation_drop = p_base_14 - p_14pm
        self.assertGreaterEqual(
            storm_perturbation_drop,
            3.0,
            f"Expected storm perturbation drop >= 3.0 hPa, got {storm_perturbation_drop:.2f} hPa",
        )

    def test_tc_s1_t3_3_02_solar_irradiance_collapse_rate(self):
        """TC-S1-T3.3-02: Solar Irradiance Collapse Rate (Lux drop > 75% in the 30 min before rain onset)."""
        # At 13:30 (0.5h before rain onset at 14:00) vs 14:00 (rain onset)
        t_base_1330, rh_base_1330, p_base_1330, p0_base_1330, lux_base_1330 = (
            self.diurnal_gen.generate_step(13.5, add_noise=False)
        )
        res_1330 = self.coordinator.apply_convective_storm(
            13.5, t_base_1330, rh_base_1330, p_base_1330, p0_base_1330, lux_base_1330, dt_hours=0.25
        )
        lux_1330 = res_1330[4]

        t_base_1400, rh_base_1400, p_base_1400, p0_base_1400, lux_base_1400 = (
            self.diurnal_gen.generate_step(14.0, add_noise=False)
        )
        res_1400 = self.coordinator.apply_convective_storm(
            14.0, t_base_1400, rh_base_1400, p_base_1400, p0_base_1400, lux_base_1400, dt_hours=0.25
        )
        lux_1400 = res_1400[4]

        # Irradiance drop between 13:30 and 14:00
        lux_drop_pct = (lux_1330 - lux_1400) / lux_1330
        self.assertGreater(
            lux_drop_pct, 0.75, f"Expected Lux collapse > 75% in last 30m, got {lux_drop_pct*100:.1f}%"
        )
        self.assertLessEqual(lux_1400, 5000.0, f"Expected storm Lux <= 5000 Lux, got {lux_1400}")

    def test_tc_s1_t3_3_03_humidity_surge_verification(self):
        """TC-S1-T3.3-03: Humidity Surge Verification (RH >= 95.0% at rain onset)."""
        t_base, rh_base, p_base, p0_base, lux_base = self.diurnal_gen.generate_step(
            14.0, add_noise=False
        )
        res = self.coordinator.apply_convective_storm(
            14.0, t_base, rh_base, p_base, p0_base, lux_base, dt_hours=0.25
        )
        rh_at_onset = res[1]
        self.assertGreaterEqual(
            rh_at_onset,
            95.0,
            f"Expected RH >= 95.0% at storm onset, got {rh_at_onset:.2f}%",
        )

    def test_tc_s1_t3_3_04_tipping_bucket_accumulation_accuracy(self):
        """TC-S1-T3.3-04: Tipping-Bucket Accumulation Accuracy (40 mm/hr for 30 min = 20.0 mm = 100 tips)."""
        coord = StormEventCoordinator()
        # 30 steps of 1-minute intervals (dt = 1/60 hours) @ 40.0 mm/hr
        dt_hours = 1.0 / 60.0
        total_tips = 0
        total_tipped_mm = 0.0

        for _ in range(30):
            tips, tipped_mm = coord.process_rain_step(rain_rate_mmh=40.0, dt_hours=dt_hours)
            total_tips += tips
            total_tipped_mm += tipped_mm

        # 40 mm/hr * 0.5 hr = 20.0 mm; 20.0 mm / 0.20 mm/tip = exactly 100 tips
        self.assertEqual(total_tips, 100, f"Expected exactly 100 tips, got {total_tips}")
        self.assertAlmostEqual(total_tipped_mm, 20.0, places=2)
        self.assertAlmostEqual(coord.rain_bucket_reservoir_mm, 0.0, places=4)

    def test_tc_s1_t3_3_05_false_alarm_cloud_shadow_isolation(self):
        """TC-S1-T3.3-05: False-Alarm Cloud Shadow Isolation (Lux drop > 75%, Delta P < 0.3 hPa, RH < 65%)."""
        engine = MicroclimateEngine(
            duration_days=1, interval_min=15, scenario="false_alarm_cloud_shadow", seed=42
        )
        states = engine.run()

        # Shadow occurs between 13:00 and 13:30 (hours 13.0 to 13.5 -> steps 52 to 54)
        # Step at 13:15 (step 53, hour 13.25)
        step_1315 = [s for s in states if abs(s.elapsed_hours - 13.25) < 0.01][0]
        fair_engine = MicroclimateEngine(
            duration_days=1, interval_min=15, scenario="fair_weather", seed=42
        )
        fair_states = fair_engine.run()
        fair_1315 = [s for s in fair_states if abs(s.elapsed_hours - 13.25) < 0.01][0]

        # 1. Lux drops by 80% (which is > 75%)
        lux_reduction = (fair_1315.solar_lux - step_1315.solar_lux) / fair_1315.solar_lux
        self.assertGreater(lux_reduction, 0.75)
        self.assertAlmostEqual(lux_reduction, 0.80, delta=0.02)

        # 2. Barometric pressure remains undisturbed (difference between shadow and fair is 0.0)
        self.assertAlmostEqual(step_1315.pressure_hpa, fair_1315.pressure_hpa, delta=0.2)

        # 3. Relative humidity does not surge (RH < 65% at mid-day)
        self.assertLess(step_1315.humidity_pct, 65.0)

        # 4. Zero rain and zero lead time
        self.assertEqual(step_1315.is_raining, 0)
        self.assertEqual(step_1315.rain_rate_mmh, 0.0)
        self.assertEqual(step_1315.rain_gauge_tip_count, 0)
        self.assertEqual(step_1315.ground_truth_lead_time_min, 0.0)

    def test_tc_s1_t3_3_06_lead_time_monotonicity(self):
        """TC-S1-T3.3-06: Lead Time Monotonicity (decrements monotonically from 150 min to 0 min)."""
        engine = MicroclimateEngine(
            duration_days=1, interval_min=15, scenario="pre_monsoon_convective", seed=42
        )
        states = engine.run()

        # Filter build-up window: 11:30 to 14:00 (elapsed_hours from 11.5 to 14.0)
        buildup_states = [s for s in states if 11.5 <= s.elapsed_hours <= 14.0]
        lead_times = [s.ground_truth_lead_time_min for s in buildup_states]

        # Start at 150 min (or near 150 min), end at 0.0 at rain onset
        self.assertAlmostEqual(lead_times[0], 150.0, delta=1.0)
        self.assertEqual(lead_times[-1], 0.0)

        # Monotonically non-increasing
        for i in range(len(lead_times) - 1):
            self.assertGreater(
                lead_times[i],
                lead_times[i + 1],
                f"Lead time not strictly decreasing: {lead_times[i]} -> {lead_times[i+1]}",
            )

    def test_tc_s1_t3_3_07_false_alarm_orographic_fog(self):
        """TC-S1-T3.3-07: False-Alarm Orographic Fog (high RH >= 98%, low Lux < 5000 Lux, rising pressure)."""
        engine = MicroclimateEngine(
            duration_days=1, interval_min=15, scenario="false_alarm_orographic_fog", seed=42
        )
        states = engine.run()

        # Morning fog: 06:00 to 09:00 (hours 6.0 to 9.0)
        fog_states = [s for s in states if 6.0 <= s.elapsed_hours <= 9.0]
        for s in fog_states:
            self.assertGreaterEqual(s.humidity_pct, 98.0)
            self.assertLessEqual(s.solar_lux, 5000.0)
            self.assertEqual(s.is_raining, 0)
            self.assertEqual(s.rain_rate_mmh, 0.0)

        # Pressure should be rising during the morning tide from 06:00 to 09:00
        p_0600 = [s for s in states if abs(s.elapsed_hours - 6.0) < 0.01][0].pressure_hpa
        p_0900 = [s for s in states if abs(s.elapsed_hours - 9.0) < 0.01][0].pressure_hpa
        self.assertGreater(p_0900, p_0600, f"Expected rising morning pressure, got {p_0600} -> {p_0900}")

    def test_tc_s1_t3_3_08_monsoon_sustained_scenario(self):
        """TC-S1-T3.3-08: Monsoon Sustained Scenario (continuous rain, high RH >= 95%, low Lux)."""
        engine = MicroclimateEngine(
            duration_days=1, interval_min=15, scenario="monsoon_sustained", seed=42
        )
        states = engine.run()

        for s in states:
            self.assertEqual(s.is_raining, 1)
            self.assertGreaterEqual(s.humidity_pct, 95.0)
            self.assertLessEqual(s.solar_lux, 8000.0)
            self.assertGreaterEqual(s.rain_rate_mmh, 2.0)
            self.assertLessEqual(s.rain_rate_mmh, 8.0)

        # Total accumulated rain after 24 hours should be substantial (~100 mm)
        final_state = states[-1]
        self.assertGreater(final_state.accumulated_rain_mm, 50.0)
        self.assertGreater(final_state.rain_gauge_tip_count, 250)

    def test_tc_s1_t3_3_09_multi_day_storm_cycle(self):
        """TC-S1-T3.3-09: Multi-Day Scenario Cycling (7-day simulation covering distinct daily scenarios)."""
        engine = MicroclimateEngine(
            duration_days=7, interval_min=15, scenario="multi_day_storm_cycle", seed=42
        )
        states = engine.run()
        self.assertEqual(len(states), 672)

        # Day 0: fair weather (0-24h) -> no rain
        day0_states = [s for s in states if s.elapsed_hours < 24.0]
        self.assertTrue(all(s.is_raining == 0 for s in day0_states))

        # Day 1: convective storm (24-48h) -> rain occurs at 14:00 (hour 38)
        day1_rain = [s for s in states if 24.0 <= s.elapsed_hours < 48.0 and s.is_raining == 1]
        self.assertTrue(len(day1_rain) > 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)

