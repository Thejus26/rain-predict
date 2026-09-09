# LoRaWAN Telemetry Protocol & Binary Payload Specification

## 1. Overview & Protocol Objectives

The **Tea Plantation Rain Prediction System** utilizes a high-efficiency, bit-packed binary telemetry format designed for **LoRaWAN Class A** end-devices operating on standard Sub-GHz ISM bands (EU868, US915, IN865).

### Protocol Design Goals
- **Minimal Time-on-Air (ToA)**: Total payload length is fixed at **12 bytes** ($96\text{ bits}$), achieving $< 50\text{ ms}$ transmission duration at SF7/125kHz to preserve battery and maintain regional duty-cycle compliance ($< 1\%$).
- **Deterministic Field Decoding**: Floating-point values are scaled into compact fixed-point integers with explicit physical boundary clamping.
- **Bi-Directional Configuration**: Supports downlink packets on dedicated LoRaWAN application ports (FPort 10) for adjusting station elevation, sampling periods, and alert thresholds in the field.

---

## 2. LoRaWAN Port Allocation

| FPort | Direction | Payload Type | Description |
| :---: | :---: | :--- | :--- |
| **FPort 1** | Uplink | Periodic Environmental Telemetry | Regular scheduled measurement and forecast packet ($12\text{ bytes}$). |
| **FPort 2** | Uplink | Emergency Storm Warning Uplink | Transmitted immediately when $CPI \ge 70\text{\%}$ or physical rain begins ($12\text{ bytes}$). |
| **FPort 10**| Downlink| Node Configuration & Parameter Set | Remote calibration, elevation update, and sampling interval configuration. |

---

## 3. 12-Byte Uplink Payload Structure (FPort 1 & FPort 2)

```
Byte 0      Byte 1      Byte 2      Byte 3      Byte 4      Byte 5
┌───────────┬───────────┬───────────┬───────────┬───────────┬───────────┐
│ Temperature (int16_t) │  Humidity (uint16_t)  │  Pressure (uint16_t)  │
│  Scale: 0.01 °C       │  Scale: 0.01 %RH      │  Offset: 300, 0.02 hPa│
└───────────┴───────────┴───────────┴───────────┴───────────┴───────────┘

Byte 6      Byte 7      Byte 8      Byte 9      Byte 10     Byte 11
┌───────────┬───────────┬───────────┬───────────┬───────────┬───────────┐
│ Ambient Lux (uint16_t)│ Rain (u8) │State & Zam│ CPI & Drop│Bat & Flags│
│  Scale: 2.0 Lux       │ 0.2 mm/LSB│ Bitfield  │ Bitfield  │ Bitfield  │
└───────────┴───────────┴───────────┴───────────┴───────────┴───────────┘
```

---

### 3.1 Field-by-Field Encoding Specification

| Byte Offset | Field Name | Data Type | Range | Resolution / Scale | Offset | Decoded Formula |
| :---: | :--- | :---: | :---: | :---: | :---: | :--- |
| **0–1** | **Temperature ($T$)** | Signed 16-bit (`int16_t` BE) | $-40.00^\circ\text{C}$ to $+85.00^\circ\text{C}$ | $0.01^\circ\text{C}$ | $0.0$ | $T = \text{Raw} / 100.0$ |
| **2–3** | **Relative Humidity ($RH$)** | Unsigned 16-bit (`uint16_t` BE) | $0.00\%$ to $100.00\%$ | $0.01\%$ | $0.0$ | $RH = \text{Raw} / 100.0$ |
| **4–5** | **Barometric Pressure ($P$)** | Unsigned 16-bit (`uint16_t` BE) | $300.00\text{ to }1100.00\text{ hPa}$ | $0.02\text{ hPa}$ | $300.0\text{ hPa}$ | $P = 300.0 + (\text{Raw} \times 0.02)$ |
| **6–7** | **Ambient Solar Lux ($Lux$)** | Unsigned 16-bit (`uint16_t` BE) | $0\text{ to }83,000\text{ Lux}$ | $2.0\text{ Lux}$ | $0$ | $Lux = \text{Raw} \times 2$ |
| **8** | **Interval Rain ($R_{\text{int}}$)** | Unsigned 8-bit (`uint8_t`) | $0.0\text{ to }51.0\text{ mm}$ | $0.2\text{ mm}$ | $0.0$ | $R_{\text{int}} = \text{Raw} \times 0.2$ |
| **9** | **Forecast State & Zambretti** | Unsigned 8-bit Bitfield | See Section 3.2 | — | — | Bit-packed state + Zambretti index |
| **10** | **CPI & Solar Alarm** | Unsigned 8-bit Bitfield | See Section 3.3 | — | — | Bit-packed CPI score + solar flag |
| **11** | **Battery Voltage & Diagnostics**| Unsigned 8-bit Bitfield | See Section 3.4 | — | — | Bit-packed $V_{\text{bat}}$ + error flags |

---

### 3.2 Byte 9 Bitfield: Forecast State & Zambretti Index

```
  Bit 7      Bit 6      Bit 5      Bit 4      Bit 3      Bit 2      Bit 1      Bit 0
┌──────────────────────┬────────────────────────────────────────────────────────────┐
│ Forecast State [1:0] │              Zambretti Index [5:0] (1..26)                 │
└──────────────────────┴────────────────────────────────────────────────────────────┘
```

| Bits | Field | Description | Encoding |
| :---: | :--- | :--- | :--- |
| **7:6** | **Forecast State** | Operational rain forecast status | `00`: Unlikely, `01`: Possible, `10`: Imminent, `11`: Active Rain |
| **5:0** | **Zambretti Index** | Heuristic index ($1\text{ to }26$) | Raw integer value ($1 = \text{Settled Fine}, 26 = \text{Severe Storm}$) |

---

### 3.3 Byte 10 Bitfield: Composite Precipitation Index & Solar Drop

```
  Bit 7      Bit 6      Bit 5      Bit 4      Bit 3      Bit 2      Bit 1      Bit 0
┌──────────┬────────────────────────────────────────────────────────────────────────┐
│Solar Drop│            Composite Precipitation Index (CPI) [6:0] (0..100%)         │
└──────────┴────────────────────────────────────────────────────────────────────────┘
```

| Bits | Field | Description | Encoding |
| :---: | :--- | :--- | :--- |
| **7** | **Solar Cloud Drop** | Daylight storm cloud attenuation flag | `0`: Normal daylight/night, `1`: Sudden severe attenuation ($> 50\%$) |
| **6:0** | **CPI Probability** | Composite Rain Probability ($0\text{–}100\%$) | Direct integer percentage value ($0\text{ to }100$) |

---

### 3.4 Byte 11 Bitfield: Battery Level & Diagnostic Flags

```
  Bit 7      Bit 6      Bit 5      Bit 4      Bit 3      Bit 2      Bit 1      Bit 0
┌──────────┬──────────┬─────────────────────────────────────────────────────────────┐
│ RST Flag │Sensor Err│   Battery Voltage V_bat [5:0] (2.50V to 3.76V, Step: 20mV)  │
└──────────┴──────────┴─────────────────────────────────────────────────────────────┘
```

| Bits | Field | Description | Encoding |
| :---: | :--- | :--- | :--- |
| **7** | **Reset Cause** | Unexpected reset indicator | `0`: Clean RTC/LPTIM wake, `1`: Watchdog / Brownout reset |
| **6** | **Sensor Fault** | Sensor communication error | `0`: Normal operation, `1`: Bus timeout / CRC error |
| **5:0** | **Battery Voltage** | Cell terminal voltage ($V_{\text{bat}}$) | $V_{\text{bat}} = 2.50\text{ V} + (\text{raw} \times 0.020\text{ V})$ |

*(e.g., Value $40 \implies 2.50\text{V} + (40 \times 0.020\text{V}) = 3.30\text{ V}$)*


---

## 4. Downlink Configuration Commands (FPort 10)

Downlink messages sent from the local estate server or gateway allow remote parameter adjustment:

| Cmd ID | Name | Payload Bytes | Schema & Arguments |
| :---: | :--- | :---: | :--- |
| **`0x01`** | **Set Sampling Interval** | 3 bytes | `[0x01, Interval_High_Byte, Interval_Low_Byte]` (Seconds: $60\text{–}3600\text{ s}$). |
| **`0x02`** | **Set Elevation Offset** | 3 bytes | `[0x02, Altitude_High_Byte, Altitude_Low_Byte]` (Meters: $0\text{–}3000\text{ m}$). |
| **`0x03`** | **Set Alert Thresholds** | 3 bytes | `[0x03, CPI_Watch_Pct, CPI_Alert_Pct]` (e.g. `[0x03, 40, 70]`). |
| **`0x04`** | **Trigger System Action** | 2 bytes | `[0x04, ActionCode]` (`0x01` = Software Reset, `0x02` = Force LoRaWAN Re-join). |

---

## 5. Official Decoders

### 5.1 JavaScript Payload Formatter (ChirpStack / TTN)
Targeting [`tools/decoders/payload_decoder.js`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tools/decoders/payload_decoder.js):

```javascript
function decodeUplink(input) {
    var bytes = input.bytes;
    if (bytes.length < 12) {
        return { errors: ["Payload too short (< 12 bytes)"] };
    }

    // Temperature (signed 16-bit BE, 0.01 °C)
    var rawTemp = (bytes[0] << 8) | bytes[1];
    if (rawTemp & 0x8000) rawTemp -= 0x10000;
    var temperature = rawTemp / 100.0;

    // Relative Humidity (unsigned 16-bit BE, 0.01 %)
    var humidity = ((bytes[2] << 8) | bytes[3]) / 100.0;

    // Barometric Pressure (unsigned 16-bit BE, offset 300, 0.02 hPa)
    var rawPress = (bytes[4] << 8) | bytes[5];
    var pressure = 300.0 + (rawPress * 0.02);

    // Ambient Lux (unsigned 16-bit BE, scale 2.0 Lux)
    var lux = ((bytes[6] << 8) | bytes[7]) * 2.0;

    // Interval Rain (0.2 mm/step)
    var rainMm = bytes[8] * 0.2;

    // Byte 9: State & Zambretti
    var forecastStateCode = (bytes[9] >> 6) & 0x03;
    var zambrettiIndex = bytes[9] & 0x3F;
    var stateNames = ["UNLIKELY", "POSSIBLE", "IMMINENT", "ACTIVE_RAIN"];

    // Byte 10: CPI & Solar Flag
    var solarDropAlarm = (bytes[10] & 0x80) !== 0;
    var cpiScore = bytes[10] & 0x7F;

    // Byte 11: Battery & Diagnostics
    var resetFlag = (bytes[11] & 0x80) !== 0;
    var sensorError = (bytes[11] & 0x40) !== 0;
    var batteryVoltage = 2.50 + ((bytes[11] & 0x3F) * 0.020);

    return {
        data: {
            temperature_c: Number(temperature.toFixed(2)),
            humidity_pct: Number(humidity.toFixed(2)),
            pressure_hpa: Number(pressure.toFixed(2)),
            ambient_lux: lux,
            rain_interval_mm: Number(rainMm.toFixed(2)),
            forecast_state: stateNames[forecastStateCode],
            zambretti_index: zambrettiIndex,
            composite_rain_prob_pct: cpiScore,
            solar_cloud_drop_alarm: solarDropAlarm,
            battery_v: Number(batteryVoltage.toFixed(3)),
            sensor_fault: sensorError,
            unexpected_reset: resetFlag
        }
    };
}
```

---

### 5.2 Python Decoder Reference
Targeting [`tools/decoders/payload_decoder.py`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/tools/decoders/payload_decoder.py):

```python
import struct

def decode_payload(payload_bytes: bytes) -> dict:
    if len(payload_bytes) < 12:
        raise ValueError("Payload must be at least 12 bytes")

    raw_temp, raw_rh, raw_press, raw_lux, raw_rain, b9, b10, b11 = struct.unpack(
        ">hHHHBBBB", payload_bytes[:12]
    )

    state_names = {0: "UNLIKELY", 1: "POSSIBLE", 2: "IMMINENT", 3: "ACTIVE_RAIN"}

    return {
        "temperature_c": round(raw_temp / 100.0, 2),
        "humidity_pct": round(raw_rh / 100.0, 2),
        "pressure_hpa": round(300.0 + (raw_press * 0.02), 2),
        "ambient_lux": float(raw_lux * 2),
        "rain_interval_mm": round(raw_rain * 0.2, 2),
        "forecast_state": state_names.get((b9 >> 6) & 0x03, "UNKNOWN"),
        "zambretti_index": b9 & 0x3F,
        "composite_rain_prob_pct": b10 & 0x7F,
        "solar_cloud_drop_alarm": bool(b10 & 0x80),
        "battery_v": round(2.50 + ((b11 & 0x3F) * 0.020), 3),
        "sensor_fault": bool(b11 & 0x40),
        "unexpected_reset": bool(b11 & 0x80),
    }
```
