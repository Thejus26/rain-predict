/**
 * @file    ttn_decoder.js
 * @brief   The Things Network (TTN v3 / The Things Stack) JavaScript Payload Formatter.
 * @details Decodes FPort 1 (12-byte periodic) and FPort 2 (4-byte alert) uplinks; encodes and decodes FPort 10 downlinks.
 *          Target environment: The Things Stack v3 Payload Formatter engine (QuickJS / V8) and Node.js.
 */

// =============================================================================
// Constants & Lookup Tables
// =============================================================================

var STATE_NAMES = ["UNLIKELY", "POSSIBLE", "IMMINENT", "ACTIVE_RAIN"];

var TRIGGER_CAUSES = [
    "MANUAL",
    "CPI_THRESHOLD",
    "PRESSURE_PLUNGE",
    "SOLAR_COLLAPSE",
    "FIRST_RAIN_TIP",
    "COMBINED_GRADIENT",
    "DEW_CONVERGENCE",
    "LOW_BATTERY"
];

var RAIN_INTENSITIES = ["NONE", "LIGHT", "MODERATE", "HEAVY"];

var ACTION_NAMES = {
    1: "REBOOT",
    2: "REJOIN"
};

var ZAMBRETTI_DICTIONARY = {
    1:  { code: "A", text: "Settled Fine" },
    2:  { code: "B", text: "Fine Weather" },
    3:  { code: "C", text: "Becoming Fine" },
    4:  { code: "D", text: "Fine, Becoming Less Settled" },
    5:  { code: "E", text: "Fine, Possible Showers" },
    6:  { code: "F", text: "Fairly Fine, Improving" },
    7:  { code: "G", text: "Fairly Fine, Possible Showers Early" },
    8:  { code: "H", text: "Fairly Fine, Showers Later" },
    9:  { code: "I", text: "Showers Early, Improving" },
    10: { code: "J", text: "Changeable, Mending" },
    11: { code: "K", text: "Fairly Fine, Showers Likely" },
    12: { code: "L", text: "Rather Unsettled, Clearing Later" },
    13: { code: "M", text: "Unsettled, Probably Improving" },
    14: { code: "N", text: "Showers Early, Showers Later" },
    15: { code: "O", text: "Showers at Times, Becoming Less Settled" },
    16: { code: "P", text: "Changeable, Some Rain" },
    17: { code: "Q", text: "Unsettled, Short Fine Intervals" },
    18: { code: "R", text: "Unsettled, Rain at Times" },
    19: { code: "S", text: "Unsettled, Rain Increasing" },
    20: { code: "T", text: "Rain at Times, Worse Later" },
    21: { code: "U", text: "Rain at Times, Becoming Very Unsettled" },
    22: { code: "V", text: "Rain at Frequent Intervals" },
    23: { code: "W", text: "Very Unsettled, Rain" },
    24: { code: "X", text: "Stormy, Much Rain" },
    25: { code: "Y", text: "Very Stormy, Heavy Rain" },
    26: { code: "Z", text: "Severe Storm, Gale / Torrential Rain" }
};

// =============================================================================
// Uplink Decoder Implementation
// =============================================================================

/**
 * @brief The Things Stack v3 entry point for uplink decoding.
 * @param {object} input { bytes: number[], fPort: number, recvTime: string }
 * @returns {object} { data: object, warnings: string[], errors: string[] }
 */
function decodeUplink(input) {
    var warnings = [];
    var errors = [];
    var data = {};

    if (!input || !Array.isArray(input.bytes)) {
        errors.push("Invalid input object or missing bytes array");
        return { data: data, warnings: warnings, errors: errors };
    }

    var bytes = input.bytes;
    var fPort = (input.fPort !== undefined && input.fPort !== null) ? input.fPort : 1;

    if (fPort === 1) {
        // ---------------------------------------------------------------------
        // FPort 1: Periodic Environmental Telemetry (12 Bytes)
        // ---------------------------------------------------------------------
        if (bytes.length < 12) {
            errors.push("FPort 1 payload too short: expected 12 bytes, received " + bytes.length);
            return { data: data, warnings: warnings, errors: errors };
        }

        // Temperature (Signed 16-bit BE, 0.01 °C)
        var rawTemp = (bytes[0] << 8) | bytes[1];
        if (rawTemp & 0x8000) {
            rawTemp -= 0x10000;
        }
        var tempC = rawTemp / 100.0;

        // Relative Humidity (Unsigned 16-bit BE, 0.01 %RH)
        var rawHum = (bytes[2] << 8) | bytes[3];
        var humPct = rawHum / 100.0;

        // Barometric Pressure (Unsigned 16-bit BE, Offset 300.0, 0.02 hPa)
        var rawPress = (bytes[4] << 8) | bytes[5];
        var pressHpa = 300.0 + (rawPress * 0.02);

        // Ambient Solar Lux (Unsigned 16-bit BE, Scale 2.0 Lux)
        var rawLux = (bytes[6] << 8) | bytes[7];
        var lux = rawLux * 2.0;

        // Interval Accumulated Rain (Unsigned 8-bit, 0.20 mm)
        var rainMm = bytes[8] * 0.20;

        // Byte 9: Forecast State [7:6] & Zambretti Index [5:0]
        var stateCode = (bytes[9] >> 6) & 0x03;
        var zambrettiIdx = bytes[9] & 0x3F;
        var zamInfo = ZAMBRETTI_DICTIONARY[zambrettiIdx] || { code: "?", text: "Unknown" };

        // Byte 10: Solar Drop Flag [7] & CPI Probability [6:0]
        var solarAlarm = (bytes[10] & 0x80) !== 0;
        var cpiPct = bytes[10] & 0x7F;

        // Byte 11: Unexpected Reset [7], Sensor Fault [6], Battery Vbat [5:0] (20mV step, 2.50V offset)
        var rstFlag = (bytes[11] & 0x80) !== 0;
        var sensorErr = (bytes[11] & 0x40) !== 0;
        var rawVbat = bytes[11] & 0x3F;
        var batteryV = 2.50 + (rawVbat * 0.020);

        if (bytes.length > 12) {
            warnings.push("Payload length (" + bytes.length + " bytes) exceeds expected 12 bytes; extra bytes ignored");
        }

        data = {
            temperature_c: Number(tempC.toFixed(2)),
            humidity_pct: Number(humPct.toFixed(2)),
            pressure_hpa: Number(pressHpa.toFixed(2)),
            ambient_lux: Number(lux.toFixed(1)),
            rain_interval_mm: Number(rainMm.toFixed(2)),
            forecast_state: STATE_NAMES[stateCode] || "UNKNOWN",
            forecast_state_code: stateCode,
            zambretti_code: zamInfo.code,
            zambretti_index: zambrettiIdx,
            zambretti_text: zamInfo.text,
            composite_rain_prob_pct: cpiPct,
            solar_cloud_drop_alarm: solarAlarm,
            battery_voltage_v: Number(batteryV.toFixed(3)),
            sensor_fault: sensorErr,
            unexpected_reset: rstFlag
        };

    } else if (fPort === 2) {
        // ---------------------------------------------------------------------
        // FPort 2: Urgent Storm Alert Warning (4 Bytes)
        // ---------------------------------------------------------------------
        if (bytes.length < 4) {
            errors.push("FPort 2 payload too short: expected 4 bytes, received " + bytes.length);
            return { data: data, warnings: warnings, errors: errors };
        }

        // Byte 0: State [7:6], Trigger Cause [5:3], Sequence Token [2:0]
        var alertStateCode = (bytes[0] >> 6) & 0x03;
        var triggerCode = (bytes[0] >> 3) & 0x07;
        var seqId = bytes[0] & 0x07;

        // Byte 1: Solar Drop Flag [7], CPI Probability [6:0]
        var alertSolarAlarm = (bytes[1] & 0x80) !== 0;
        var alertCpiPct = bytes[1] & 0x7F;

        // Byte 2: Barometric Pressure Rate dP/dt (Signed 8-bit, 0.05 hPa/hr)
        var rawRate = bytes[2];
        if (rawRate & 0x80) {
            rawRate -= 0x100;
        }
        var pressRate = rawRate * 0.05;

        // Byte 3: Rain Intensity [7:6], Sensor Fault [5], Battery Vbat [4:0] (40mV step, 2.50V offset)
        var rainTierCode = (bytes[3] >> 6) & 0x03;
        var alertSensorErr = (bytes[3] & 0x20) !== 0;
        var alertRawVbat = bytes[3] & 0x1F;
        var alertBatteryV = 2.50 + (alertRawVbat * 0.040);

        if (bytes.length > 4) {
            warnings.push("Payload length (" + bytes.length + " bytes) exceeds expected 4 bytes; extra bytes ignored");
        }

        data = {
            alert_state: STATE_NAMES[alertStateCode] || "UNKNOWN",
            alert_state_code: alertStateCode,
            trigger_cause: TRIGGER_CAUSES[triggerCode] || "UNKNOWN",
            trigger_cause_code: triggerCode,
            alert_sequence_id: seqId,
            composite_rain_prob_pct: alertCpiPct,
            solar_cloud_drop_alarm: alertSolarAlarm,
            pressure_rate_hpa_per_h: Number(pressRate.toFixed(2)),
            rain_intensity: RAIN_INTENSITIES[rainTierCode] || "UNKNOWN",
            rain_intensity_code: rainTierCode,
            sensor_fault: alertSensorErr,
            battery_voltage_v: Number(alertBatteryV.toFixed(3))
        };

    } else {
        errors.push("Unsupported fPort: " + fPort + " (expected fPort 1 or 2)");
    }

    return {
        data: data,
        warnings: warnings,
        errors: errors
    };
}

// =============================================================================
// Downlink Encoder Implementation
// =============================================================================

/**
 * @brief The Things Stack v3 entry point for downlink encoding.
 * @param {object} input { data: object }
 * @returns {object} { bytes: number[], fPort: number, warnings: string[], errors: string[] }
 */
function encodeDownlink(input) {
    var warnings = [];
    var errors = [];
    var bytes = [];

    if (!input || !input.data) {
        errors.push("Missing input.data object");
        return { bytes: bytes, fPort: 10, warnings: warnings, errors: errors };
    }

    var data = input.data;

    if (data.interval_sec !== undefined) {
        // Command 0x01: Set Sampling Interval (Seconds: 60..3600)
        var interval = Math.max(60, Math.min(3600, Math.round(data.interval_sec)));
        bytes = [0x01, (interval >> 8) & 0xFF, interval & 0xFF];
    } else if (data.elevation_m !== undefined) {
        // Command 0x02: Set Station Elevation (Meters: 0..3000)
        var elev = Math.max(0, Math.min(3000, Math.round(data.elevation_m)));
        bytes = [0x02, (elev >> 8) & 0xFF, elev & 0xFF];
    } else if (data.cpi_watch_pct !== undefined && data.cpi_alert_pct !== undefined) {
        // Command 0x03: Set Alert Thresholds (CPI Watch %, CPI Alert %)
        var watch = Math.max(0, Math.min(100, Math.round(data.cpi_watch_pct)));
        var alert = Math.max(0, Math.min(100, Math.round(data.cpi_alert_pct)));
        bytes = [0x03, watch & 0xFF, alert & 0xFF];
    } else if (data.action_code !== undefined) {
        // Command 0x04: Trigger System Action (1 = Reset, 2 = Rejoin)
        var action = Math.round(data.action_code) & 0xFF;
        bytes = [0x04, action];
    } else {
        errors.push("Unrecognized command payload schema in input.data");
    }

    return {
        bytes: bytes,
        fPort: 10,
        warnings: warnings,
        errors: errors
    };
}

// =============================================================================
// Downlink Decoder Implementation
// =============================================================================

/**
 * @brief The Things Stack v3 entry point for downlink decoding.
 * @param {object} input { bytes: number[], fPort: number }
 * @returns {object} { data: object, warnings: string[], errors: string[] }
 */
function decodeDownlink(input) {
    var warnings = [];
    var errors = [];
    var data = {};

    if (!input || !Array.isArray(input.bytes)) {
        errors.push("Missing input.bytes array");
        return { data: data, warnings: warnings, errors: errors };
    }

    var bytes = input.bytes;
    if (bytes.length < 2) {
        errors.push("Downlink payload too short (< 2 bytes)");
        return { data: data, warnings: warnings, errors: errors };
    }

    var cmdId = bytes[0];

    if (cmdId === 0x01 && bytes.length >= 3) {
        var interval = (bytes[1] << 8) | bytes[2];
        data = {
            command: "SET_SAMPLING_INTERVAL",
            interval_sec: interval
        };
    } else if (cmdId === 0x02 && bytes.length >= 3) {
        var elevation = (bytes[1] << 8) | bytes[2];
        data = {
            command: "SET_STATION_ELEVATION",
            elevation_m: elevation
        };
    } else if (cmdId === 0x03 && bytes.length >= 3) {
        data = {
            command: "SET_ALERT_THRESHOLDS",
            cpi_watch_pct: bytes[1],
            cpi_alert_pct: bytes[2]
        };
    } else if (cmdId === 0x04) {
        var actionCode = bytes[1];
        data = {
            command: "TRIGGER_SYSTEM_ACTION",
            action_code: actionCode,
            action: ACTION_NAMES[actionCode] || "UNKNOWN"
        };
    } else {
        errors.push("Unknown or incomplete downlink command: 0x" + cmdId.toString(16));
    }

    return {
        data: data,
        warnings: warnings,
        errors: errors
    };
}

// =============================================================================
// CommonJS Module Exports (for Node.js Unit Testing)
// =============================================================================

if (typeof module !== "undefined" && module.exports) {
    module.exports = {
        decodeUplink: decodeUplink,
        encodeDownlink: encodeDownlink,
        decodeDownlink: decodeDownlink,
        STATE_NAMES: STATE_NAMES,
        TRIGGER_CAUSES: TRIGGER_CAUSES,
        RAIN_INTENSITIES: RAIN_INTENSITIES,
        ACTION_NAMES: ACTION_NAMES,
        ZAMBRETTI_DICTIONARY: ZAMBRETTI_DICTIONARY
    };
}
