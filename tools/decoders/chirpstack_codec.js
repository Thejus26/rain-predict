/**
 * @file    chirpstack_codec.js
 * @brief   ChirpStack v3 & v4 JavaScript Payload Codec for Tea Plantation Rain Prediction System.
 * @details Decodes FPort 1 (12-byte periodic) and FPort 2 (4-byte alert) uplinks; encodes FPort 10 downlinks.
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

var ZAMBRETTI_TEXTS = {
    1:  { letter: "A", text: "Settled Fine" },
    2:  { letter: "B", text: "Fine Weather" },
    3:  { letter: "C", text: "Becoming Fine" },
    4:  { letter: "D", text: "Fine, Becoming Less Settled" },
    5:  { letter: "E", text: "Fine, Possible Showers" },
    6:  { letter: "F", text: "Fairly Fine, Improving" },
    7:  { letter: "G", text: "Fairly Fine, Possible Showers Early" },
    8:  { letter: "H", text: "Fairly Fine, Showers Later" },
    9:  { letter: "I", text: "Showers Early, Improving" },
    10: { letter: "J", text: "Changeable, Mending" },
    11: { letter: "K", text: "Fairly Fine, Showers Likely" },
    12: { letter: "L", text: "Rather Unsettled, Clearing Later" },
    13: { letter: "M", text: "Unsettled, Probably Improving" },
    14: { letter: "N", text: "Showers Early, Showers Later" },
    15: { letter: "O", text: "Showers at Times, Becoming Less Settled" },
    16: { letter: "P", text: "Changeable, Some Rain" },
    17: { letter: "Q", text: "Unsettled, Short Fine Intervals" },
    18: { letter: "R", text: "Unsettled, Rain at Times" },
    19: { letter: "S", text: "Unsettled, Rain Increasing" },
    20: { letter: "T", text: "Rain at Times, Worse Later" },
    21: { letter: "U", text: "Rain at Times, Becoming Very Unsettled" },
    22: { letter: "V", text: "Rain at Frequent Intervals" },
    23: { letter: "W", text: "Very Unsettled, Rain" },
    24: { letter: "X", text: "Stormy, Much Rain" },
    25: { letter: "Y", text: "Very Stormy, Heavy Rain" },
    26: { letter: "Z", text: "Severe Storm, Gale / Torrential Rain" }
};

// =============================================================================
// Internal Uplink Decoder Functions
// =============================================================================

/**
 * @brief Decodes 12-byte periodic environmental telemetry packet (FPort 1).
 * @param {Array<number>} bytes Raw binary payload.
 * @returns {object} { data: {...}, errors: [...], warnings: [...] }
 */
function decodePeriodicTelemetry(bytes) {
    if (!bytes || bytes.length < 12) {
        return {
            data: {},
            errors: ["Periodic payload too short (expected 12 bytes, got " + (bytes ? bytes.length : 0) + ")"],
            warnings: []
        };
    }

    // Bytes 0..1: Temperature (Signed 16-bit Big-Endian, 0.01 °C)
    var rawTemp = (bytes[0] << 8) | bytes[1];
    if (rawTemp & 0x8000) {
        rawTemp -= 0x10000;
    }
    var temperature = rawTemp / 100.0;

    // Bytes 2..3: Relative Humidity (Unsigned 16-bit Big-Endian, 0.01 %RH)
    var rawHum = (bytes[2] << 8) | bytes[3];
    var humidity = rawHum / 100.0;

    // Bytes 4..5: Barometric Pressure (Unsigned 16-bit Big-Endian, Offset 300.0, 0.02 hPa)
    var rawPress = (bytes[4] << 8) | bytes[5];
    var pressure = 300.0 + (rawPress * 0.02);

    // Bytes 6..7: Ambient Solar Lux (Unsigned 16-bit Big-Endian, Scale 2.0 Lux)
    var rawLux = (bytes[6] << 8) | bytes[7];
    var lux = rawLux * 2.0;

    // Byte 8: Interval Accumulated Rain (Unsigned 8-bit, 0.20 mm)
    var rainMm = bytes[8] * 0.20;

    // Byte 9: Forecast State [7:6] & Zambretti Index [5:0]
    var stateCode = (bytes[9] >> 6) & 0x03;
    var zambrettiIdx = bytes[9] & 0x3F;
    var zamInfo = ZAMBRETTI_TEXTS[zambrettiIdx] || { letter: "?", text: "Unknown" };

    // Byte 10: Solar Drop Flag [7] & CPI Probability [6:0]
    var solarAlarm = (bytes[10] & 0x80) !== 0;
    var cpiPct = bytes[10] & 0x7F;

    // Byte 11: Unexpected Reset [7], Sensor Fault [6], Battery Vbat [5:0] (20mV step, 2.50V offset)
    var rstFlag = (bytes[11] & 0x80) !== 0;
    var sensorErr = (bytes[11] & 0x40) !== 0;
    var rawVbat = bytes[11] & 0x3F;
    var batteryV = 2.50 + (rawVbat * 0.020);

    return {
        data: {
            temperature_c: Number(temperature.toFixed(2)),
            humidity_pct: Number(humidity.toFixed(2)),
            pressure_hpa: Number(pressure.toFixed(2)),
            ambient_lux: Number(lux.toFixed(1)),
            rain_interval_mm: Number(rainMm.toFixed(2)),
            forecast_state: STATE_NAMES[stateCode] || "UNKNOWN",
            forecast_state_code: stateCode,
            zambretti_index: zambrettiIdx,
            zambretti_letter: zamInfo.letter,
            zambretti_text: zamInfo.text,
            composite_rain_prob_pct: cpiPct,
            solar_cloud_drop_alarm: solarAlarm,
            battery_v: Number(batteryV.toFixed(3)),
            sensor_fault: sensorErr,
            unexpected_reset: rstFlag
        },
        errors: [],
        warnings: []
    };
}

/**
 * @brief Decodes 4-byte urgent storm alert warning packet (FPort 2).
 * @param {Array<number>} bytes Raw binary payload.
 * @returns {object} { data: {...}, errors: [...], warnings: [...] }
 */
function decodeUrgentAlert(bytes) {
    if (!bytes || bytes.length < 4) {
        return {
            data: {},
            errors: ["Alert payload too short (expected 4 bytes, got " + (bytes ? bytes.length : 0) + ")"],
            warnings: []
        };
    }

    // Byte 0: State [7:6], Trigger Cause [5:3], Sequence Token [2:0]
    var stateCode = (bytes[0] >> 6) & 0x03;
    var triggerCode = (bytes[0] >> 3) & 0x07;
    var seqId = bytes[0] & 0x07;

    // Byte 1: Solar Drop Flag [7], CPI Probability [6:0]
    var solarAlarm = (bytes[1] & 0x80) !== 0;
    var cpiPct = bytes[1] & 0x7F;

    // Byte 2: Barometric Pressure Rate dP/dt (Signed 8-bit, 0.05 hPa/hr)
    var rawRate = bytes[2];
    if (rawRate & 0x80) {
        rawRate -= 0x100;
    }
    var pressRate = rawRate * 0.05;

    // Byte 3: Rain Intensity [7:6], Sensor Fault [5], Battery Vbat [4:0] (40mV step, 2.50V offset)
    var rainTierCode = (bytes[3] >> 6) & 0x03;
    var sensorErr = (bytes[3] & 0x20) !== 0;
    var rawVbat = bytes[3] & 0x1F;
    var batteryV = 2.50 + (rawVbat * 0.040);

    return {
        data: {
            alert_state: STATE_NAMES[stateCode] || "UNKNOWN",
            alert_state_code: stateCode,
            trigger_cause: TRIGGER_CAUSES[triggerCode] || "UNKNOWN",
            trigger_cause_code: triggerCode,
            alert_sequence_id: seqId,
            composite_rain_prob_pct: cpiPct,
            solar_cloud_drop_alarm: solarAlarm,
            pressure_rate_hpa_per_h: Number(pressRate.toFixed(2)),
            rain_intensity: RAIN_INTENSITIES[rainTierCode] || "UNKNOWN",
            rain_intensity_code: rainTierCode,
            sensor_fault: sensorErr,
            battery_v: Number(batteryV.toFixed(3))
        },
        errors: [],
        warnings: []
    };
}

// =============================================================================
// ChirpStack v4 Interface Functions
// =============================================================================

/**
 * @brief ChirpStack v4 entry point for uplink decoding.
 * @param {object} input { bytes: [ ... ], fPort: number, variables: { ... } }
 * @returns {object} { data: { ... }, errors: [ ... ], warnings: [ ... ] }
 */
function decodeUplink(input) {
    if (!input || !input.bytes) {
        return {
            data: {},
            errors: ["Invalid input object or missing bytes array"],
            warnings: []
        };
    }

    var fPort = input.fPort || 1;
    var bytes = input.bytes;

    if (fPort === 1) {
        return decodePeriodicTelemetry(bytes);
    } else if (fPort === 2) {
        return decodeUrgentAlert(bytes);
    } else {
        return {
            data: {},
            errors: ["Unsupported fPort: " + fPort + ". Expected fPort 1 (periodic) or 2 (alert)."],
            warnings: []
        };
    }
}

/**
 * @brief ChirpStack v4 entry point for downlink encoding.
 * @param {object} input { data: { ... }, variables: { ... } }
 * @returns {object} { bytes: [ ... ], fPort: 10, errors: [ ... ], warnings: [ ... ] }
 */
function encodeDownlink(input) {
    if (!input || !input.data) {
        return {
            bytes: [],
            fPort: 10,
            errors: ["Missing input data object for downlink encoding"],
            warnings: []
        };
    }

    var data = input.data;
    var bytes = [];

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
        return {
            bytes: [],
            fPort: 10,
            errors: ["Unrecognized downlink command schema in input data"],
            warnings: []
        };
    }

    return {
        bytes: bytes,
        fPort: 10,
        errors: [],
        warnings: []
    };
}

// =============================================================================
// ChirpStack v3 Legacy Compatibility Functions
// =============================================================================

function Decode(fPort, bytes, variables) {
    var result = decodeUplink({ bytes: bytes, fPort: fPort, variables: variables });
    if (result.errors && result.errors.length > 0) {
        return { error: result.errors.join("; ") };
    }
    return result.data;
}

function Encode(fPort, obj, variables) {
    var result = encodeDownlink({ data: obj, variables: variables });
    if (result.errors && result.errors.length > 0) {
        return [];
    }
    return result.bytes;
}

// =============================================================================
// CommonJS Module Export (for Automated Node.js Testing)
// =============================================================================

if (typeof module !== "undefined" && module.exports) {
    module.exports = {
        decodeUplink: decodeUplink,
        encodeDownlink: encodeDownlink,
        Decode: Decode,
        Encode: Encode,
        STATE_NAMES: STATE_NAMES,
        TRIGGER_CAUSES: TRIGGER_CAUSES,
        RAIN_INTENSITIES: RAIN_INTENSITIES,
        ZAMBRETTI_TEXTS: ZAMBRETTI_TEXTS
    };
}
