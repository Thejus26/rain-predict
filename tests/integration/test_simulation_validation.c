/**
 * @file    test_simulation_validation.c
 * @brief   Sprint 7 Task S7-T1.1: 30-Day Multi-Scenario Synthetic Climate Validation.
 * @details Executes continuous 2,880-step (30 Days @ 15-min interval) multi-regime
 *          climate simulation across pre-monsoon convective squalls, sustained monsoon fronts,
 *          morning valley fog, passing cloud shadows, and high-pressure ridges, verifying all
 *          12 simulation invariants (TC-SIM-01 through TC-SIM-12) against C99 firmware algorithms.
 */

#include "unity.h"
#include "status.h"
#include "dew_point.h"
#include "zambretti.h"
#include "trend_detector.h"
#include "rain_algo.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ========================================================================== */
/* Simulation Definitions & Buffer Allocations                                */
/* ========================================================================== */

#define SIM_TOTAL_STEPS          (2880U)     /**< 30 Days * 24 Hours * 4 Steps/Hour */
#define SIM_STEP_HOURS           (0.25f)     /**< 15 minutes = 0.25 hours */
#define SIM_ELEVATION_M          (1500.0f)   /**< Tea Estate Elevation (AMSL) */
#define SIM_HORIZON_STEPS        (8U)        /**< 120-min Prediction Horizon = 8 * 15m */

typedef struct {
    uint32_t step_index;
    float    timestamp_hour;
    uint32_t day_index;
    float    temp_c;
    float    humidity_pct;
    float    pressure_hpa;
    float    solar_lux;
    uint32_t rain_gauge_tips;
    float    rain_rate_mmh;
    float    accumulated_rain_mm;
    uint8_t  phase_id;                       /**< 1=Convective, 2=Monsoon, 3=FalseAlarm, 4=FairWeather */
} sim_climate_sample_t;

typedef struct {
    uint32_t event_id;
    uint32_t start_step;
    uint32_t end_step;
    float    start_hour;
    float    total_rain_mm;
    float    peak_intensity_mmh;
    uint8_t  phase_id;
    float    lead_time_min;
    bool     is_hit;
} sim_rain_event_t;

#define MAX_RAIN_EVENTS          (32U)

/* Static buffers allocated in BSS (zero heap allocation) */
static sim_climate_sample_t s_climate_data[SIM_TOTAL_STEPS];
static rain_forecast_t      s_forecasts[SIM_TOTAL_STEPS];
static multi_gradient_t     s_gradients[SIM_TOTAL_STEPS];
static sim_rain_event_t     s_rain_events[MAX_RAIN_EVENTS];
static uint32_t             s_rain_event_count = 0U;

/* ========================================================================== */
/* Synthetic Climate Generator                                                */
/* ========================================================================== */

static float sim_calc_barometric_altitude_p(float p0_hpa, float temp_c, float elevation_m) {
    float temp_k = temp_c + 273.15f;
    float lapse_rate = 0.0065f;
    float t_sea = temp_k + lapse_rate * elevation_m;
    float base = 1.0f - (lapse_rate * elevation_m) / t_sea;
    return p0_hpa * powf(base, 5.257f);
}

static void sim_generate_30day_dataset(void) {
    float accumulated_rain = 0.0f;
    const float base_p0 = 1013.25f;
    (void)base_p0;

    for (uint32_t step = 0; step < SIM_TOTAL_STEPS; step++) {
        float t_hours = (float)step * SIM_STEP_HOURS;
        uint32_t day = (uint32_t)(t_hours / 24.0f) + 1U;
        float hour_of_day = fmodf(t_hours, 24.0f);

        uint8_t phase_id;
        if (day <= 7U) {
            phase_id = 1U;
        } else if (day <= 15U) {
            phase_id = 2U;
        } else if (day <= 22U) {
            phase_id = 3U;
        } else {
            phase_id = 4U;
        }

        float solar_zenith = 0.0f;
        if (hour_of_day >= 6.0f && hour_of_day <= 18.0f) {
            solar_zenith = sinf((float)M_PI * (hour_of_day - 6.0f) / 12.0f);
            if (solar_zenith < 0.0f) {
                solar_zenith = 0.0f;
            }
        }

        float temp_c = 20.0f;
        float rh_pct = 60.0f;
        float p0 = 1013.25f;
        float lux = 0.0f;
        float rain_rate = 0.0f;

        if (phase_id == 1U) {
            /* Phase 1: Pre-Monsoon Convective Squalls (Days 1–7) */
            float t_min = 16.0f, t_max = 28.0f;
            float rh_min = 50.0f, rh_max = 88.0f;
            float p0_base = 1012.0f;
            float lux_peak = 95000.0f;

            temp_c = t_min + (t_max - t_min) * (0.5f + 0.5f * sinf((float)M_PI * (hour_of_day - 9.0f) / 12.0f));
            rh_pct = rh_max - (rh_max - rh_min) * (0.5f + 0.5f * sinf((float)M_PI * (hour_of_day - 9.0f) / 12.0f));
            lux = solar_zenith * lux_peak;
            p0 = p0_base - 1.5f * sinf(2.0f * (float)M_PI * hour_of_day / 24.0f);

            /* Injected convective storms on Days 2, 3, 5, 7 between 12:00 and 17:00 */
            if ((day == 2U || day == 3U || day == 5U || day == 7U) &&
                (hour_of_day >= 12.0f && hour_of_day <= 17.0f)) {
                float storm_rel_t = hour_of_day - 12.0f;
                if (storm_rel_t < 2.5f) {
                    /* Precursor build-up window (12:00 - 14:30) */
                    float prog = storm_rel_t / 2.5f;
                    p0 -= 5.2f * powf(prog, 1.6f);
                    rh_pct += (96.0f - rh_pct) * powf(prog, 0.95f);
                    if (rh_pct > 96.0f) {
                        rh_pct = 96.0f;
                    }
                    if (storm_rel_t >= 0.5f) {
                        float cloud_prog = (storm_rel_t - 0.5f) / 2.0f;
                        if (cloud_prog > 1.0f) {
                            cloud_prog = 1.0f;
                        }
                        float atten = 1.0f - 0.96f * powf(cloud_prog, 0.70f);
                        if (atten < 0.015f) {
                            atten = 0.015f;
                        }
                        lux *= atten;
                    }
                    temp_c -= 4.5f * powf(prog, 0.9f);
                } else if (storm_rel_t <= 3.5f) {
                    /* Active cloudburst (14:30 - 15:30) */
                    p0 -= 5.2f;
                    lux = 1200.0f;
                    rh_pct = 98.0f;
                    temp_c -= 4.8f;
                    rain_rate = (day == 2U || day == 7U) ? 38.0f : 24.0f;
                } else {
                    /* Dissipation (15:30 - 17:00) */
                    float recovery = (storm_rel_t - 3.5f) / 1.5f;
                    p0 = p0_base - 5.2f * (1.0f - recovery);
                    float r_lux = solar_zenith * lux_peak * recovery;
                    lux = (r_lux > 1000.0f) ? r_lux : 1000.0f;
                    rh_pct = 98.0f - 20.0f * recovery;
                    rain_rate = 0.0f;
                }
            }
        } else if (phase_id == 2U) {
            /* Phase 2: Active Monsoon Front (Days 8–15) */
            float t_min = 17.0f, t_max = 20.5f;
            temp_c = t_min + (t_max - t_min) * (0.5f + 0.5f * sinf((float)M_PI * (hour_of_day - 9.0f) / 12.0f));
            rh_pct = 93.0f + 5.0f * sinf((float)M_PI * hour_of_day / 24.0f);
            if (rh_pct > 100.0f) {
                rh_pct = 100.0f;
            }
            lux = solar_zenith * 10500.0f;
            p0 = 998.0f + 2.0f * sinf(2.0f * (float)M_PI * (t_hours / 72.0f));

            bool is_raining = (((uint32_t)t_hours) % 18U) < 10U;
            rain_rate = is_raining ? 8.5f : 0.0f;
        } else if (phase_id == 3U) {
            /* Phase 3: False-Alarm Perturbation (Days 16–22) */
            float t_min = 14.0f, t_max = 26.0f;
            temp_c = t_min + (t_max - t_min) * (0.5f + 0.5f * sinf((float)M_PI * (hour_of_day - 9.0f) / 12.0f));
            rh_pct = 50.0f + 15.0f * (1.0f - solar_zenith);
            p0 = 1018.5f + 1.2f * cosf((float)M_PI * hour_of_day / 12.0f);
            lux = solar_zenith * 90000.0f;
            rain_rate = 0.0f;

            /* Morning Valley Radiation Fog (Days 16, 17, 19 from 02:00 to 09:00) */
            if ((day == 16U || day == 17U || day == 19U) && (hour_of_day >= 2.0f && hour_of_day <= 9.0f)) {
                float fog_env = sinf((float)M_PI * (hour_of_day - 2.0f) / 7.0f);
                rh_pct += 25.0f * fog_env;
                if (rh_pct > 88.0f) {
                    rh_pct = 88.0f;
                }
                p0 += 1.2f * fog_env;
                if (hour_of_day >= 6.0f) {
                    float max_fog_lux = 2400.0f + (hour_of_day - 6.0f) * 8000.0f;
                    if (lux > max_fog_lux) {
                        lux = max_fog_lux;
                    }
                }
            }

            /* Passing Cloud Shadows (Days 18, 20, 21 from 13:00 to 14:00) */
            if ((day == 18U || day == 20U || day == 21U) && (hour_of_day >= 13.0f && hour_of_day <= 14.0f)) {
                lux = 11000.0f;
            }
        } else {
            /* Phase 4: High-Pressure Ridge & Transitions (Days 23–30) */
            float t_min = 15.0f, t_max = 27.5f;
            temp_c = t_min + (t_max - t_min) * (0.5f + 0.5f * sinf((float)M_PI * (hour_of_day - 9.0f) / 12.0f));

            if (day <= 27U) {
                rh_pct = 38.0f + 18.0f * (1.0f - solar_zenith);
                p0 = 1023.5f + 0.8f * cosf((float)M_PI * hour_of_day / 12.0f);
                lux = solar_zenith * 108000.0f;
                rain_rate = 0.0f;
            } else {
                rh_pct = 45.0f + 25.0f * (1.0f - solar_zenith);
                p0 = 1013.5f;
                lux = solar_zenith * 95000.0f;
                rain_rate = 0.0f;

                if (hour_of_day >= 15.5f && hour_of_day <= 17.5f) {
                    float rel_t = hour_of_day - 15.5f;
                    if (rel_t < 0.75f) {
                        p0 -= 2.4f * (rel_t / 0.75f);
                        rh_pct += 32.0f * (rel_t / 0.75f);
                        if (rh_pct > 94.0f) {
                            rh_pct = 94.0f;
                        }
                        lux *= 0.12f;
                    } else if (rel_t <= 1.5f) {
                        p0 -= 2.4f;
                        rain_rate = 22.0f;
                        rh_pct = 96.0f;
                        lux = 1500.0f;
                    } else {
                        rain_rate = 0.0f;
                    }
                }
            }
        }

        float pressure_local = sim_calc_barometric_altitude_p(p0, temp_c, SIM_ELEVATION_M);
        float step_rain = rain_rate * SIM_STEP_HOURS;
        accumulated_rain += step_rain;
        uint32_t tips = (uint32_t)roundf(step_rain / 0.20f);

        s_climate_data[step].step_index          = step;
        s_climate_data[step].timestamp_hour      = t_hours;
        s_climate_data[step].day_index           = day;
        s_climate_data[step].temp_c              = temp_c;
        s_climate_data[step].humidity_pct        = (rh_pct > 100.0f) ? 100.0f : ((rh_pct < 35.0f) ? 35.0f : rh_pct);
        s_climate_data[step].pressure_hpa        = pressure_local;
        s_climate_data[step].solar_lux           = (lux > 0.0f) ? lux : 0.0f;
        s_climate_data[step].rain_gauge_tips     = tips;
        s_climate_data[step].rain_rate_mmh       = rain_rate;
        s_climate_data[step].accumulated_rain_mm = accumulated_rain;
        s_climate_data[step].phase_id            = phase_id;
    }
}

static void sim_extract_rain_events(void) {
    s_rain_event_count = 0U;
    bool in_event = false;
    uint32_t start_step = 0U;
    float event_rain = 0.0f;
    float peak_rate = 0.0f;
    uint32_t dry_steps = 0U;

    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        if (s_climate_data[i].rain_rate_mmh > 0.0f || s_climate_data[i].rain_gauge_tips > 0U) {
            if (!in_event) {
                in_event = true;
                start_step = i;
                event_rain = 0.0f;
                peak_rate = 0.0f;
                dry_steps = 0U;
            }
            event_rain += (float)s_climate_data[i].rain_gauge_tips * 0.20f;
            if (s_climate_data[i].rain_rate_mmh > peak_rate) {
                peak_rate = s_climate_data[i].rain_rate_mmh;
            }
            dry_steps = 0U;
        } else {
            if (in_event) {
                dry_steps++;
                if (dry_steps >= 4U) {  /* 60 min dry = event end */
                    if (event_rain >= 0.40f && s_rain_event_count < MAX_RAIN_EVENTS) {
                        s_rain_events[s_rain_event_count].event_id           = s_rain_event_count + 1U;
                        s_rain_events[s_rain_event_count].start_step         = start_step;
                        s_rain_events[s_rain_event_count].end_step           = i - 4U;
                        s_rain_events[s_rain_event_count].start_hour         = s_climate_data[start_step].timestamp_hour;
                        s_rain_events[s_rain_event_count].total_rain_mm      = event_rain;
                        s_rain_events[s_rain_event_count].peak_intensity_mmh = peak_rate;
                        s_rain_events[s_rain_event_count].phase_id           = s_climate_data[start_step].phase_id;
                        s_rain_events[s_rain_event_count].lead_time_min      = 0.0f;
                        s_rain_events[s_rain_event_count].is_hit             = false;
                        s_rain_event_count++;
                    }
                    in_event = false;
                }
            }
        }
    }
}

static void sim_run_firmware_nowcasting(void) {
    /* Sliding history buffer matching app_state_machine.c */
    env_sample_t history[TREND_SAMPLES_3HOUR + 1U];
    uint32_t history_count = 0U;

    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        float p0_sea = 0.0f;
        status_t rc_p0 = dew_point_calc_sea_level_pressure(s_climate_data[i].pressure_hpa,
                                                            s_climate_data[i].temp_c,
                                                            SIM_ELEVATION_M,
                                                            &p0_sea);
        if (rc_p0 != STATUS_OK) {
            p0_sea = s_climate_data[i].pressure_hpa;
        }

        env_sample_t new_sample;
        new_sample.temp_c      = s_climate_data[i].temp_c;
        new_sample.rh_pct      = s_climate_data[i].humidity_pct;
        new_sample.p0_hpa      = p0_sea;
        new_sample.lux         = s_climate_data[i].solar_lux;
        new_sample.timestamp_s = i * 900U;

        if (history_count < (TREND_SAMPLES_3HOUR + 1U)) {
            history[history_count] = new_sample;
            history_count++;
        } else {
            for (uint32_t k = 0; k < TREND_SAMPLES_3HOUR; k++) {
                history[k] = history[k + 1U];
            }
            history[TREND_SAMPLES_3HOUR] = new_sample;
        }

        (void)trend_detector_compute_gradients(history, history_count, &s_gradients[i]);
        (void)rain_algo_evaluate(history,
                                 history_count,
                                 0.0f,
                                 6U,
                                 WIND_DIR_CALM,
                                 0.0f,
                                 &s_forecasts[i]);
    }
}

static void sim_correlate_events(void) {
    for (uint32_t e = 0; e < s_rain_event_count; e++) {
        uint32_t ev_start = s_rain_events[e].start_step;
        uint32_t lookback = (ev_start >= SIM_HORIZON_STEPS) ? (ev_start - SIM_HORIZON_STEPS) : 0U;
        int32_t alert_step = -1;

        for (uint32_t i = lookback; i < ev_start; i++) {
            if (s_forecasts[i].forecast_state >= RAIN_ALERT_LIKELY) {
                alert_step = (int32_t)i;
                break;
            }
        }

        if (alert_step != -1) {
            s_rain_events[e].lead_time_min = (float)(ev_start - (uint32_t)alert_step) * 15.0f;
            s_rain_events[e].is_hit = true;
        } else {
            s_rain_events[e].lead_time_min = 0.0f;
            s_rain_events[e].is_hit = false;
        }
    }
}

/* ========================================================================== */
/* Unity Setup & Teardown                                                     */
/* ========================================================================== */

void setUp(void) {
    /* Generated once globally per test suite */
}

void tearDown(void) {
    /* No dynamically allocated resources to tear down */
}

/* ========================================================================== */
/* 12 Simulation Invariant Test Cases (TC-SIM-01 .. TC-SIM-12)               */
/* ========================================================================== */

static void test_sim_01_continuous_30day_stepping(void) {
    TEST_ASSERT_EQUAL_UINT32(2880U, SIM_TOTAL_STEPS);
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        TEST_ASSERT_FALSE(isnan(s_climate_data[i].temp_c));
        TEST_ASSERT_FALSE(isnan(s_climate_data[i].humidity_pct));
        TEST_ASSERT_FALSE(isnan(s_climate_data[i].pressure_hpa));
        TEST_ASSERT_FALSE(isnan(s_climate_data[i].solar_lux));
        TEST_ASSERT_FALSE(isnan(s_forecasts[i].cpi_score_pct));
    }
}

static void test_sim_02_physical_boundary_invariants(void) {
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        TEST_ASSERT_TRUE(s_climate_data[i].temp_c >= 10.0f && s_climate_data[i].temp_c <= 36.0f);
        TEST_ASSERT_TRUE(s_climate_data[i].humidity_pct >= 35.0f && s_climate_data[i].humidity_pct <= 100.0f);
        TEST_ASSERT_TRUE(s_climate_data[i].pressure_hpa >= 800.0f && s_climate_data[i].pressure_hpa <= 1030.0f);
        TEST_ASSERT_TRUE(s_climate_data[i].solar_lux >= 0.0f && s_climate_data[i].solar_lux <= 120000.0f);
    }
}

static void test_sim_03_dew_point_mathematical_bounds(void) {
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        float tdew = 0.0f;
        float dpd  = 0.0f;
        TEST_ASSERT_EQUAL_INT(STATUS_OK, dew_point_calc_tdew(s_climate_data[i].temp_c, s_climate_data[i].humidity_pct, &tdew));
        TEST_ASSERT_EQUAL_INT(STATUS_OK, dew_point_calc_depression(s_climate_data[i].temp_c, s_climate_data[i].humidity_pct, &dpd));
        TEST_ASSERT_TRUE(tdew <= (s_climate_data[i].temp_c + 0.01f));
        TEST_ASSERT_TRUE(dpd >= 0.0f);
    }
}

static void test_sim_04_convective_cloudburst_detection(void) {
    uint32_t phase1_storm_count = 0U;
    for (uint32_t e = 0; e < s_rain_event_count; e++) {
        if (s_rain_events[e].phase_id == 1U) {
            phase1_storm_count++;
            TEST_ASSERT_TRUE(s_rain_events[e].is_hit);
            TEST_ASSERT_TRUE(s_rain_events[e].lead_time_min >= 60.0f);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, phase1_storm_count);
}

static void test_sim_05_precursor_optical_attenuation(void) {
    uint32_t blackout_drop_count = 0U;
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        float hour = fmodf(s_climate_data[i].timestamp_hour, 24.0f);
        if (hour >= 11.0f && hour <= 16.5f) {
            if (s_gradients[i].solar_drop_pct_30m >= 70.0f && s_climate_data[i].solar_lux < 3000.0f) {
                blackout_drop_count++;
                TEST_ASSERT_TRUE(s_forecasts[i].score_solar >= 85U);
            }
        }
    }
    TEST_ASSERT_TRUE(blackout_drop_count > 0U);
}

static void test_sim_06_severe_barometric_override(void) {
    uint32_t override_count = 0U;
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        if (s_gradients[i].delta_p_1h_hpa <= CRITICAL_DROP_1H_HPA &&
            s_climate_data[i].humidity_pct >= CRITICAL_DROP_RH_MIN_PCT) {
            override_count++;
            TEST_ASSERT_EQUAL_INT(RAIN_ALERT_IMMINENT, s_forecasts[i].forecast_state);
        }
    }
    TEST_ASSERT_TRUE(override_count > 0U);
}

static void test_sim_07_sustained_monsoon_tracking(void) {
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        if (s_climate_data[i].phase_id == 2U) {
            /* Must never false-clear down to UNLIKELY during sustained monsoon */
            TEST_ASSERT_TRUE(s_forecasts[i].forecast_state != RAIN_ALERT_UNLIKELY);
        }
    }
}

static void test_sim_08_morning_valley_fog_rejection(void) {
    uint32_t fog_step_count = 0U;
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        uint32_t day = s_climate_data[i].day_index;
        float hour = fmodf(s_climate_data[i].timestamp_hour, 24.0f);
        if ((day == 16U || day == 17U || day == 19U) && (hour >= 2.0f && hour <= 9.0f)) {
            fog_step_count++;
            TEST_ASSERT_TRUE(s_forecasts[i].cpi_score_pct < 40.0f);
            TEST_ASSERT_TRUE(s_forecasts[i].forecast_state != RAIN_ALERT_IMMINENT);
        }
    }
    TEST_ASSERT_TRUE(fog_step_count > 0U);
}

static void test_sim_09_passing_cloud_shadow_rejection(void) {
    uint32_t shadow_step_count = 0U;
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        uint32_t day = s_climate_data[i].day_index;
        float hour = fmodf(s_climate_data[i].timestamp_hour, 24.0f);
        if ((day == 18U || day == 20U || day == 21U) && (hour >= 13.0f && hour <= 14.0f)) {
            shadow_step_count++;
            TEST_ASSERT_TRUE(s_forecasts[i].cpi_score_pct < 35.0f);
            TEST_ASSERT_TRUE(s_forecasts[i].forecast_state == RAIN_ALERT_UNLIKELY);
        }
    }
    TEST_ASSERT_TRUE(shadow_step_count > 0U);
}

static void test_sim_10_fair_weather_quiescent_stability(void) {
    uint32_t ridge_step_count = 0U;
    for (uint32_t i = 0; i < SIM_TOTAL_STEPS; i++) {
        if (s_climate_data[i].day_index >= 23U && s_climate_data[i].day_index <= 27U) {
            ridge_step_count++;
            TEST_ASSERT_TRUE(s_forecasts[i].cpi_score_pct < 15.0f);
            TEST_ASSERT_TRUE(s_forecasts[i].z_index <= 4U);
            TEST_ASSERT_TRUE(s_forecasts[i].forecast_state == RAIN_ALERT_UNLIKELY);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(480U, ridge_step_count);
}

static void test_sim_11_deterministic_seed_repeatability(void) {
    /* Store first cycle snapshot */
    float first_cpi_day2_storm = s_forecasts[150].cpi_score_pct;

    /* Re-generate and re-evaluate */
    sim_generate_30day_dataset();
    sim_run_firmware_nowcasting();

    TEST_ASSERT_FLOAT_WITHIN(0.001f, first_cpi_day2_storm, s_forecasts[150].cpi_score_pct);
}

static void test_sim_12_host_execution_performance(void) {
    clock_t start = clock();

    /* Benchmark full 2,880-step generation, gradient math, and algorithm evaluation */
    sim_generate_30day_dataset();
    sim_run_firmware_nowcasting();

    clock_t end = clock();
    double cpu_time_sec = ((double)(end - start)) / CLOCKS_PER_SEC;

    /* Must complete well within the 3.0 second specification limit */
    TEST_ASSERT_TRUE(cpu_time_sec < 3.0);
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    /* Initialize dataset and run nowcasting pipeline */
    sim_generate_30day_dataset();
    sim_extract_rain_events();
    sim_run_firmware_nowcasting();
    sim_correlate_events();

    UNITY_BEGIN();
    RUN_TEST(test_sim_01_continuous_30day_stepping);
    RUN_TEST(test_sim_02_physical_boundary_invariants);
    RUN_TEST(test_sim_03_dew_point_mathematical_bounds);
    RUN_TEST(test_sim_04_convective_cloudburst_detection);
    RUN_TEST(test_sim_05_precursor_optical_attenuation);
    RUN_TEST(test_sim_06_severe_barometric_override);
    RUN_TEST(test_sim_07_sustained_monsoon_tracking);
    RUN_TEST(test_sim_08_morning_valley_fog_rejection);
    RUN_TEST(test_sim_09_passing_cloud_shadow_rejection);
    RUN_TEST(test_sim_10_fair_weather_quiescent_stability);
    RUN_TEST(test_sim_11_deterministic_seed_repeatability);
    RUN_TEST(test_sim_12_host_execution_performance);
    return UNITY_END();
}
