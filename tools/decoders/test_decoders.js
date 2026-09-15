/**
 * @file    test_decoders.js
 * @brief   Automated Node.js test suite for ChirpStack and TTN payload codecs.
 * @details Validates decoding and encoding against physical and hexadecimal truth vectors.
 *          Zero external npm dependencies (uses native Node.js assert and path).
 */

const assert = require("assert");
const path = require("path");

// Load decoder modules under test
const chirpstack = require(path.join(__dirname, "chirpstack_codec.js"));
const ttn = require(path.join(__dirname, "ttn_decoder.js"));

// =============================================================================
// Test Harness Utilities & ANSI Color Helpers
// =============================================================================

let totalTests = 0;
let passedTests = 0;
let failedTests = 0;

const COLORS = {
    reset: "\x1b[0m",
    green: "\x1b[32m",
    red: "\x1b[31m",
    cyan: "\x1b[36m",
    yellow: "\x1b[33m",
    bold: "\x1b[1m"
};

function runTest(testName, testFn) {
    totalTests++;
    try {
        testFn();
        passedTests++;
        console.log(`  ${COLORS.green}✓${COLORS.reset} ${testName}`);
    } catch (err) {
        failedTests++;
        console.error(`  ${COLORS.red}✗ ${testName}${COLORS.reset}`);
        console.error(`    ${COLORS.red}Error:${COLORS.reset} ${err.message}`);
        if (err.actual !== undefined && err.expected !== undefined) {
            console.error(`    ${COLORS.yellow}Expected:${COLORS.reset} ${JSON.stringify(err.expected)}`);
            console.error(`    ${COLORS.yellow}Actual:  ${COLORS.reset} ${JSON.stringify(err.actual)}`);
        }
    }
}

function assertClose(actual, expected, delta, message) {
    const diff = Math.abs(actual - expected);
    if (diff > delta) {
        assert.fail(`${message || "Value mismatch"}: actual ${actual} not within ±${delta} of expected ${expected} (diff=${diff})`);
    }
}

// =============================================================================
// Test Suite 1: Periodic Environmental Telemetry (FPort 1, 12 Bytes)
// =============================================================================

console.log(`\n${COLORS.bold}${COLORS.cyan}=== Suite 1: Periodic Environmental Telemetry (FPort 1) ===${COLORS.reset}`);

// Vector 1: Nominal Tropical Daytime
// Hex: 09 92 21 4D 7E 13 57 E4 00 4E 2D 28
const V1_BYTES = [0x09, 0x92, 0x21, 0x4D, 0x7E, 0x13, 0x57, 0xE4, 0x00, 0x4E, 0x2D, 0x28];

runTest("ChirpStack v4 - Vector 1: Nominal Tropical Daytime", () => {
    const result = chirpstack.decodeUplink({ bytes: V1_BYTES, fPort: 1 });
    assert.strictEqual(result.errors.length, 0, "ChirpStack v4 errors should be empty");
    const d = result.data;
    assertClose(d.temperature_c, 24.50, 0.01, "Temperature");
    assertClose(d.humidity_pct, 85.25, 0.01, "Humidity");
    assertClose(d.pressure_hpa, 945.50, 0.02, "Pressure");
    assertClose(d.ambient_lux, 45000.0, 1.0, "Lux");
    assertClose(d.rain_interval_mm, 0.0, 0.01, "Rain");
    assert.strictEqual(d.forecast_state, "POSSIBLE");
    assert.strictEqual(d.forecast_state_code, 1);
    assert.strictEqual(d.zambretti_index, 14);
    assert.strictEqual(d.zambretti_letter, "N");
    assert.strictEqual(d.zambretti_text, "Showers Early, Showers Later");
    assert.strictEqual(d.composite_rain_prob_pct, 45);
    assert.strictEqual(d.solar_cloud_drop_alarm, false);
    assertClose(d.battery_v, 3.300, 0.01, "Battery Voltage");
    assert.strictEqual(d.sensor_fault, false);
    assert.strictEqual(d.unexpected_reset, false);
});

runTest("TTN v3 - Vector 1: Nominal Tropical Daytime", () => {
    const result = ttn.decodeUplink({ bytes: V1_BYTES, fPort: 1 });
    assert.strictEqual(result.errors.length, 0, "TTN v3 errors should be empty");
    const d = result.data;
    assertClose(d.temperature_c, 24.50, 0.01, "Temperature");
    assertClose(d.humidity_pct, 85.25, 0.01, "Humidity");
    assertClose(d.pressure_hpa, 945.50, 0.02, "Pressure");
    assertClose(d.ambient_lux, 45000.0, 1.0, "Lux");
    assertClose(d.rain_interval_mm, 0.0, 0.01, "Rain");
    assert.strictEqual(d.forecast_state, "POSSIBLE");
    assert.strictEqual(d.forecast_state_code, 1);
    assert.strictEqual(d.zambretti_code, "N");
    assert.strictEqual(d.zambretti_index, 14);
    assert.strictEqual(d.zambretti_text, "Showers Early, Showers Later");
    assert.strictEqual(d.composite_rain_prob_pct, 45);
    assert.strictEqual(d.solar_cloud_drop_alarm, false);
    assertClose(d.battery_voltage_v, 3.300, 0.01, "Battery Voltage");
    assert.strictEqual(d.sensor_fault, false);
    assert.strictEqual(d.unexpected_reset, false);
});

// Vector 2: Sub-Zero Mountain Frost
// Hex: FB 05 27 06 63 A6 00 00 0C 96 4E 1E
const V2_BYTES = [0xFB, 0x05, 0x27, 0x06, 0x63, 0xA6, 0x00, 0x00, 0x0C, 0x96, 0x4E, 0x1E];

runTest("ChirpStack v4 - Vector 2: Sub-Zero Mountain Frost", () => {
    const result = chirpstack.decodeUplink({ bytes: V2_BYTES, fPort: 1 });
    assert.strictEqual(result.errors.length, 0, "ChirpStack v4 errors should be empty");
    const d = result.data;
    assertClose(d.temperature_c, -12.75, 0.01, "Temperature (Two's-Complement Sign)");
    assertClose(d.humidity_pct, 99.90, 0.01, "Humidity");
    assertClose(d.pressure_hpa, 810.20, 0.02, "Pressure");
    assertClose(d.ambient_lux, 0.0, 0.1, "Lux");
    assertClose(d.rain_interval_mm, 2.40, 0.01, "Rain");
    assert.strictEqual(d.forecast_state, "IMMINENT");
    assert.strictEqual(d.forecast_state_code, 2);
    assert.strictEqual(d.zambretti_index, 22);
    assert.strictEqual(d.zambretti_letter, "V");
    assert.strictEqual(d.composite_rain_prob_pct, 78);
    assert.strictEqual(d.solar_cloud_drop_alarm, false);
    assertClose(d.battery_v, 3.100, 0.01, "Battery Voltage");
    assert.strictEqual(d.sensor_fault, false);
    assert.strictEqual(d.unexpected_reset, false);
});

runTest("TTN v3 - Vector 2: Sub-Zero Mountain Frost", () => {
    const result = ttn.decodeUplink({ bytes: V2_BYTES, fPort: 1 });
    assert.strictEqual(result.errors.length, 0, "TTN v3 errors should be empty");
    const d = result.data;
    assertClose(d.temperature_c, -12.75, 0.01, "Temperature (Two's-Complement Sign)");
    assertClose(d.humidity_pct, 99.90, 0.01, "Humidity");
    assertClose(d.pressure_hpa, 810.20, 0.02, "Pressure");
    assertClose(d.ambient_lux, 0.0, 0.1, "Lux");
    assertClose(d.rain_interval_mm, 2.40, 0.01, "Rain");
    assert.strictEqual(d.forecast_state, "IMMINENT");
    assert.strictEqual(d.forecast_state_code, 2);
    assert.strictEqual(d.zambretti_code, "V");
    assert.strictEqual(d.zambretti_index, 22);
    assert.strictEqual(d.composite_rain_prob_pct, 78);
    assert.strictEqual(d.solar_cloud_drop_alarm, false);
    assertClose(d.battery_voltage_v, 3.100, 0.01, "Battery Voltage");
    assert.strictEqual(d.sensor_fault, false);
    assert.strictEqual(d.unexpected_reset, false);
});

// Vector 3: Active Rain & Hardware Fault Flags
// Hex: 07 D0 13 88 8C A0 03 E8 05 DA E4 FF
const V3_BYTES = [0x07, 0xD0, 0x13, 0x88, 0x8C, 0xA0, 0x03, 0xE8, 0x05, 0xDA, 0xE4, 0xFF];

runTest("ChirpStack v4 - Vector 3: Active Rain & Diagnostics", () => {
    const result = chirpstack.decodeUplink({ bytes: V3_BYTES, fPort: 1 });
    assert.strictEqual(result.errors.length, 0, "ChirpStack v4 errors should be empty");
    const d = result.data;
    assertClose(d.temperature_c, 20.00, 0.01, "Temperature");
    assertClose(d.humidity_pct, 50.00, 0.01, "Humidity");
    assertClose(d.pressure_hpa, 1020.00, 0.02, "Pressure");
    assertClose(d.ambient_lux, 2000.0, 1.0, "Lux");
    assertClose(d.rain_interval_mm, 1.0, 0.01, "Rain");
    assert.strictEqual(d.forecast_state, "ACTIVE_RAIN");
    assert.strictEqual(d.forecast_state_code, 3);
    assert.strictEqual(d.zambretti_index, 26);
    assert.strictEqual(d.zambretti_letter, "Z");
    assert.strictEqual(d.composite_rain_prob_pct, 100);
    assert.strictEqual(d.solar_cloud_drop_alarm, true);
    assert.strictEqual(d.sensor_fault, true);
    assert.strictEqual(d.unexpected_reset, true);
    assertClose(d.battery_v, 3.760, 0.01, "Battery Voltage");
});

runTest("TTN v3 - Vector 3: Active Rain & Diagnostics", () => {
    const result = ttn.decodeUplink({ bytes: V3_BYTES, fPort: 1 });
    assert.strictEqual(result.errors.length, 0, "TTN v3 errors should be empty");
    const d = result.data;
    assertClose(d.temperature_c, 20.00, 0.01, "Temperature");
    assertClose(d.humidity_pct, 50.00, 0.01, "Humidity");
    assertClose(d.pressure_hpa, 1020.00, 0.02, "Pressure");
    assertClose(d.ambient_lux, 2000.0, 1.0, "Lux");
    assertClose(d.rain_interval_mm, 1.0, 0.01, "Rain");
    assert.strictEqual(d.forecast_state, "ACTIVE_RAIN");
    assert.strictEqual(d.forecast_state_code, 3);
    assert.strictEqual(d.zambretti_code, "Z");
    assert.strictEqual(d.zambretti_index, 26);
    assert.strictEqual(d.composite_rain_prob_pct, 100);
    assert.strictEqual(d.solar_cloud_drop_alarm, true);
    assert.strictEqual(d.sensor_fault, true);
    assert.strictEqual(d.unexpected_reset, true);
    assertClose(d.battery_voltage_v, 3.760, 0.01, "Battery Voltage");
});

// =============================================================================
// Test Suite 2: Urgent Storm Alert Telemetry (FPort 2, 4 Bytes)
// =============================================================================

console.log(`\n${COLORS.bold}${COLORS.cyan}=== Suite 2: Urgent Storm Alert Telemetry (FPort 2) ===${COLORS.reset}`);

// Vector 4: Severe Storm Alert
// Hex: 8B D8 BA 54
const V4_BYTES = [0x8B, 0xD8, 0xBA, 0x54];

runTest("ChirpStack v4 - Vector 4: Severe Storm Alert", () => {
    const result = chirpstack.decodeUplink({ bytes: V4_BYTES, fPort: 2 });
    assert.strictEqual(result.errors.length, 0, "ChirpStack v4 errors should be empty");
    const d = result.data;
    assert.strictEqual(d.alert_state, "IMMINENT");
    assert.strictEqual(d.alert_state_code, 2);
    assert.strictEqual(d.trigger_cause, "CPI_THRESHOLD");
    assert.strictEqual(d.trigger_cause_code, 1);
    assert.strictEqual(d.alert_sequence_id, 3);
    assert.strictEqual(d.composite_rain_prob_pct, 88);
    assert.strictEqual(d.solar_cloud_drop_alarm, true);
    assertClose(d.pressure_rate_hpa_per_h, -3.50, 0.05, "Pressure Rate");
    assert.strictEqual(d.rain_intensity, "LIGHT");
    assert.strictEqual(d.rain_intensity_code, 1);
    assert.strictEqual(d.sensor_fault, false);
    assertClose(d.battery_v, 3.300, 0.02, "Battery Voltage");
});

runTest("TTN v3 - Vector 4: Severe Storm Alert", () => {
    const result = ttn.decodeUplink({ bytes: V4_BYTES, fPort: 2 });
    assert.strictEqual(result.errors.length, 0, "TTN v3 errors should be empty");
    const d = result.data;
    assert.strictEqual(d.alert_state, "IMMINENT");
    assert.strictEqual(d.alert_state_code, 2);
    assert.strictEqual(d.trigger_cause, "CPI_THRESHOLD");
    assert.strictEqual(d.trigger_cause_code, 1);
    assert.strictEqual(d.alert_sequence_id, 3);
    assert.strictEqual(d.composite_rain_prob_pct, 88);
    assert.strictEqual(d.solar_cloud_drop_alarm, true);
    assertClose(d.pressure_rate_hpa_per_h, -3.50, 0.05, "Pressure Rate");
    assert.strictEqual(d.rain_intensity, "LIGHT");
    assert.strictEqual(d.rain_intensity_code, 1);
    assert.strictEqual(d.sensor_fault, false);
    assertClose(d.battery_voltage_v, 3.300, 0.02, "Battery Voltage");
});

// Vector 5: Barometric Plunge Squall
// Hex: 95 5C 98 13
const V5_BYTES = [0x95, 0x5C, 0x98, 0x13];

runTest("ChirpStack v4 - Vector 5: Barometric Plunge Squall", () => {
    const result = chirpstack.decodeUplink({ bytes: V5_BYTES, fPort: 2 });
    assert.strictEqual(result.errors.length, 0, "ChirpStack v4 errors should be empty");
    const d = result.data;
    assert.strictEqual(d.alert_state, "IMMINENT");
    assert.strictEqual(d.alert_state_code, 2);
    assert.strictEqual(d.trigger_cause, "PRESSURE_PLUNGE");
    assert.strictEqual(d.trigger_cause_code, 2);
    assert.strictEqual(d.alert_sequence_id, 5);
    assert.strictEqual(d.composite_rain_prob_pct, 92);
    assert.strictEqual(d.solar_cloud_drop_alarm, false);
    assertClose(d.pressure_rate_hpa_per_h, -5.20, 0.05, "Pressure Rate");
    assert.strictEqual(d.rain_intensity, "NONE");
    assert.strictEqual(d.rain_intensity_code, 0);
    assert.strictEqual(d.sensor_fault, false);
    assertClose(d.battery_v, 3.260, 0.02, "Battery Voltage");
});

runTest("TTN v3 - Vector 5: Barometric Plunge Squall", () => {
    const result = ttn.decodeUplink({ bytes: V5_BYTES, fPort: 2 });
    assert.strictEqual(result.errors.length, 0, "TTN v3 errors should be empty");
    const d = result.data;
    assert.strictEqual(d.alert_state, "IMMINENT");
    assert.strictEqual(d.alert_state_code, 2);
    assert.strictEqual(d.trigger_cause, "PRESSURE_PLUNGE");
    assert.strictEqual(d.trigger_cause_code, 2);
    assert.strictEqual(d.alert_sequence_id, 5);
    assert.strictEqual(d.composite_rain_prob_pct, 92);
    assert.strictEqual(d.solar_cloud_drop_alarm, false);
    assertClose(d.pressure_rate_hpa_per_h, -5.20, 0.05, "Pressure Rate");
    assert.strictEqual(d.rain_intensity, "NONE");
    assert.strictEqual(d.rain_intensity_code, 0);
    assert.strictEqual(d.sensor_fault, false);
    assertClose(d.battery_voltage_v, 3.260, 0.02, "Battery Voltage");
});

// Vector 6: First Rain Tip Pulse Trigger
// Hex: E1 DF DC CF
const V6_BYTES = [0xE1, 0xDF, 0xDC, 0xCF];

runTest("ChirpStack v4 - Vector 6: First Rain Tip Trigger", () => {
    const result = chirpstack.decodeUplink({ bytes: V6_BYTES, fPort: 2 });
    assert.strictEqual(result.errors.length, 0, "ChirpStack v4 errors should be empty");
    const d = result.data;
    assert.strictEqual(d.alert_state, "ACTIVE_RAIN");
    assert.strictEqual(d.alert_state_code, 3);
    assert.strictEqual(d.trigger_cause, "FIRST_RAIN_TIP");
    assert.strictEqual(d.trigger_cause_code, 4);
    assert.strictEqual(d.alert_sequence_id, 1);
    assert.strictEqual(d.composite_rain_prob_pct, 95);
    assert.strictEqual(d.solar_cloud_drop_alarm, true);
    assertClose(d.pressure_rate_hpa_per_h, -1.80, 0.05, "Pressure Rate");
    assert.strictEqual(d.rain_intensity, "HEAVY");
    assert.strictEqual(d.rain_intensity_code, 3);
    assert.strictEqual(d.sensor_fault, false);
    assertClose(d.battery_v, 3.100, 0.02, "Battery Voltage");
});

runTest("TTN v3 - Vector 6: First Rain Tip Trigger", () => {
    const result = ttn.decodeUplink({ bytes: V6_BYTES, fPort: 2 });
    assert.strictEqual(result.errors.length, 0, "TTN v3 errors should be empty");
    const d = result.data;
    assert.strictEqual(d.alert_state, "ACTIVE_RAIN");
    assert.strictEqual(d.alert_state_code, 3);
    assert.strictEqual(d.trigger_cause, "FIRST_RAIN_TIP");
    assert.strictEqual(d.trigger_cause_code, 4);
    assert.strictEqual(d.alert_sequence_id, 1);
    assert.strictEqual(d.composite_rain_prob_pct, 95);
    assert.strictEqual(d.solar_cloud_drop_alarm, true);
    assertClose(d.pressure_rate_hpa_per_h, -1.80, 0.05, "Pressure Rate");
    assert.strictEqual(d.rain_intensity, "HEAVY");
    assert.strictEqual(d.rain_intensity_code, 3);
    assert.strictEqual(d.sensor_fault, false);
    assertClose(d.battery_voltage_v, 3.100, 0.02, "Battery Voltage");
});

// =============================================================================
// Test Suite 3: Downlink Encoding & Decoding (FPort 10)
// =============================================================================

console.log(`\n${COLORS.bold}${COLORS.cyan}=== Suite 3: Downlink Configuration (FPort 10) ===${COLORS.reset}`);

runTest("ChirpStack & TTN - Cmd 0x01: Set Sampling Interval (900 s)", () => {
    const csEnc = chirpstack.encodeDownlink({ data: { interval_sec: 900 } });
    const ttnEnc = ttn.encodeDownlink({ data: { interval_sec: 900 } });

    assert.strictEqual(csEnc.fPort, 10);
    assert.strictEqual(ttnEnc.fPort, 10);
    assert.deepStrictEqual(csEnc.bytes, [0x01, 0x03, 0x84]);
    assert.deepStrictEqual(ttnEnc.bytes, [0x01, 0x03, 0x84]);

    const dec = ttn.decodeDownlink({ bytes: csEnc.bytes, fPort: 10 });
    assert.strictEqual(dec.errors.length, 0);
    assert.strictEqual(dec.data.command, "SET_SAMPLING_INTERVAL");
    assert.strictEqual(dec.data.interval_sec, 900);
});

runTest("ChirpStack & TTN - Cmd 0x02: Set Station Elevation (1550 m)", () => {
    const csEnc = chirpstack.encodeDownlink({ data: { elevation_m: 1550 } });
    const ttnEnc = ttn.encodeDownlink({ data: { elevation_m: 1550 } });

    assert.strictEqual(csEnc.fPort, 10);
    assert.strictEqual(ttnEnc.fPort, 10);
    assert.deepStrictEqual(csEnc.bytes, [0x02, 0x06, 0x0E]);
    assert.deepStrictEqual(ttnEnc.bytes, [0x02, 0x06, 0x0E]);

    const dec = ttn.decodeDownlink({ bytes: csEnc.bytes, fPort: 10 });
    assert.strictEqual(dec.errors.length, 0);
    assert.strictEqual(dec.data.command, "SET_STATION_ELEVATION");
    assert.strictEqual(dec.data.elevation_m, 1550);
});

runTest("ChirpStack & TTN - Cmd 0x03: Set Alert Thresholds (40%, 70%)", () => {
    const csEnc = chirpstack.encodeDownlink({ data: { cpi_watch_pct: 40, cpi_alert_pct: 70 } });
    const ttnEnc = ttn.encodeDownlink({ data: { cpi_watch_pct: 40, cpi_alert_pct: 70 } });

    assert.strictEqual(csEnc.fPort, 10);
    assert.strictEqual(ttnEnc.fPort, 10);
    assert.deepStrictEqual(csEnc.bytes, [0x03, 0x28, 0x46]);
    assert.deepStrictEqual(ttnEnc.bytes, [0x03, 0x28, 0x46]);

    const dec = ttn.decodeDownlink({ bytes: csEnc.bytes, fPort: 10 });
    assert.strictEqual(dec.errors.length, 0);
    assert.strictEqual(dec.data.command, "SET_ALERT_THRESHOLDS");
    assert.strictEqual(dec.data.cpi_watch_pct, 40);
    assert.strictEqual(dec.data.cpi_alert_pct, 70);
});

runTest("ChirpStack & TTN - Cmd 0x04: Trigger System Action (Reboot)", () => {
    const csEnc = chirpstack.encodeDownlink({ data: { action_code: 1 } });
    const ttnEnc = ttn.encodeDownlink({ data: { action_code: 1 } });

    assert.strictEqual(csEnc.fPort, 10);
    assert.strictEqual(ttnEnc.fPort, 10);
    assert.deepStrictEqual(csEnc.bytes, [0x04, 0x01]);
    assert.deepStrictEqual(ttnEnc.bytes, [0x04, 0x01]);

    const dec = ttn.decodeDownlink({ bytes: csEnc.bytes, fPort: 10 });
    assert.strictEqual(dec.errors.length, 0);
    assert.strictEqual(dec.data.command, "TRIGGER_SYSTEM_ACTION");
    assert.strictEqual(dec.data.action_code, 1);
    assert.strictEqual(dec.data.action, "REBOOT");
});

runTest("TTN - Cmd 0x04: Trigger System Action (Rejoin)", () => {
    const csEnc = chirpstack.encodeDownlink({ data: { action_code: 2 } });
    const ttnEnc = ttn.encodeDownlink({ data: { action_code: 2 } });

    assert.deepStrictEqual(csEnc.bytes, [0x04, 0x02]);
    assert.deepStrictEqual(ttnEnc.bytes, [0x04, 0x02]);

    const dec = ttn.decodeDownlink({ bytes: ttnEnc.bytes, fPort: 10 });
    assert.strictEqual(dec.errors.length, 0);
    assert.strictEqual(dec.data.command, "TRIGGER_SYSTEM_ACTION");
    assert.strictEqual(dec.data.action_code, 2);
    assert.strictEqual(dec.data.action, "REJOIN");
});

// =============================================================================
// Test Suite 4: Defensive Error Handling & Guard Tests
// =============================================================================

console.log(`\n${COLORS.bold}${COLORS.cyan}=== Suite 4: Defensive Error Handling & Guards ===${COLORS.reset}`);

runTest("ChirpStack - Truncated Payload Rejection", () => {
    const shortFPort1 = chirpstack.decodeUplink({ bytes: [0x01, 0x02, 0x03], fPort: 1 });
    assert.ok(shortFPort1.errors.length > 0, "Should report error on short FPort 1");

    const shortFPort2 = chirpstack.decodeUplink({ bytes: [0x01, 0x02], fPort: 2 });
    assert.ok(shortFPort2.errors.length > 0, "Should report error on short FPort 2");
});

runTest("TTN - Truncated Payload Rejection", () => {
    const shortFPort1 = ttn.decodeUplink({ bytes: [0x01, 0x02, 0x03], fPort: 1 });
    assert.ok(shortFPort1.errors.length > 0, "Should report error on short FPort 1");

    const shortFPort2 = ttn.decodeUplink({ bytes: [0x01, 0x02], fPort: 2 });
    assert.ok(shortFPort2.errors.length > 0, "Should report error on short FPort 2");
});

runTest("ChirpStack & TTN - Unsupported Port Rejection", () => {
    const csResult = chirpstack.decodeUplink({ bytes: [0x01, 0x02, 0x03, 0x04], fPort: 88 });
    assert.ok(csResult.errors.length > 0, "ChirpStack should reject FPort 88");

    const ttnResult = ttn.decodeUplink({ bytes: [0x01, 0x02, 0x03, 0x04], fPort: 88 });
    assert.ok(ttnResult.errors.length > 0, "TTN should reject FPort 88");
});

runTest("ChirpStack & TTN - Null/Invalid Input Handling", () => {
    const csNull = chirpstack.decodeUplink(null);
    assert.ok(csNull.errors.length > 0, "ChirpStack should return error for null input");

    const csNoBytes = chirpstack.decodeUplink({});
    assert.ok(csNoBytes.errors.length > 0, "ChirpStack should return error for missing bytes");

    const ttnNull = ttn.decodeUplink(null);
    assert.ok(ttnNull.errors.length > 0, "TTN should return error for null input");

    const ttnNoBytes = ttn.decodeUplink({});
    assert.ok(ttnNoBytes.errors.length > 0, "TTN should return error for missing bytes");
});

runTest("Downlink Error Handling - Missing / Invalid Data", () => {
    const csDownNull = chirpstack.encodeDownlink(null);
    assert.ok(csDownNull.errors.length > 0, "ChirpStack should report error for null downlink");

    const csDownInvalid = chirpstack.encodeDownlink({ data: { unknown_cmd: 123 } });
    assert.ok(csDownInvalid.errors.length > 0, "ChirpStack should report error for unknown command");

    const ttnDownNull = ttn.encodeDownlink(null);
    assert.ok(ttnDownNull.errors.length > 0, "TTN should report error for null downlink");

    const ttnDownInvalid = ttn.encodeDownlink({ data: { unknown_cmd: 123 } });
    assert.ok(ttnDownInvalid.errors.length > 0, "TTN should report error for unknown command");

    const ttnDownDecodeShort = ttn.decodeDownlink({ bytes: [0x01] });
    assert.ok(ttnDownDecodeShort.errors.length > 0, "TTN should reject short downlink bytes");

    const ttnDownDecodeUnknown = ttn.decodeDownlink({ bytes: [0x99, 0x00] });
    assert.ok(ttnDownDecodeUnknown.errors.length > 0, "TTN should reject unknown downlink command");
});

runTest("ChirpStack v3 Legacy Function Compatibility", () => {
    const decoded = chirpstack.Decode(1, V1_BYTES, {});
    assert.strictEqual(decoded.forecast_state, "POSSIBLE");
    assertClose(decoded.temperature_c, 24.50, 0.01);

    const encoded = chirpstack.Encode(10, { interval_sec: 900 }, {});
    assert.deepStrictEqual(encoded, [0x01, 0x03, 0x84]);

    const errDecoded = chirpstack.Decode(88, [0x01], {});
    assert.ok(errDecoded.error !== undefined, "Legacy Decode should return error property for invalid fPort");

    const errEncoded = chirpstack.Encode(10, { invalid_param: 1 }, {});
    assert.deepStrictEqual(errEncoded, [], "Legacy Encode should return empty array for error");
});

// =============================================================================
// Final Test Summary & Exit Code
// =============================================================================

console.log(`\n${COLORS.bold}------------------------------------------------------------${COLORS.reset}`);
console.log(`${COLORS.bold}Test Execution Summary:${COLORS.reset}`);
console.log(`  Total Tests:  ${totalTests}`);
console.log(`  Passed:       ${COLORS.green}${passedTests}${COLORS.reset}`);
console.log(`  Failed:       ${failedTests > 0 ? COLORS.red : COLORS.green}${failedTests}${COLORS.reset}`);
console.log(`${COLORS.bold}------------------------------------------------------------${COLORS.reset}`);

if (failedTests > 0) {
    process.exit(1);
} else {
    console.log(`${COLORS.green}${COLORS.bold}✓ All JavaScript Decoder Tests Passed Successfully!${COLORS.reset}\n`);
    process.exit(0);
}
