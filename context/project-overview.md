# Project Overview: Tea Plantation Rain Prediction System

## 1. Executive Summary
The **Tea Plantation Rain Prediction System** is a low-power, embedded IoT edge-sensing solution designed to monitor microclimates and predict rainfall events within tea plantations. Due to undulating terrain, high altitudes, and localized weather dynamics in tea estates, regional weather forecasts often lack the spatial granularity required for optimal estate operations.

This system provides localized, reliable, and timely rainfall predictions to support critical agronomic decisions such as plucking schedules, fertilizer and pesticide spraying, irrigation management, and workforce dispatch.

---

## 2. Core Objectives & Key Requirements
- **On-Site Autonomy & Edge-First Operation**: The local on-site system (MCU node and local estate hub) has primary authority. Prediction calculations, data logging, and alert triggers run 100% locally on-site without depending on active internet, cellular, or cloud infrastructure.
- **Localized Rain Prediction**: Deliver dependable, short-term rain forecasts (nowcasting / 1–6 hour horizon) directly at the plantation parcel level.
- **Immediate Local Actionability**: Drive local alerts (on-node status LEDs, e-paper display, or local estate office alarm/relay outputs) to notify estate managers and field workers immediately.
- **Long-Range Low-Power Connectivity**: Operate reliably across large acreages, hilly terrain, and dense canopy cover via private local LoRa / LoRaWAN links.
- **Ultra-Low Power Autonomy**: Run uninterrupted in the field using battery and solar energy harvesting with minimal maintenance.
- **Robust Field Sensing**: Utilize dependable, high-precision industrial/branded sensors suited for humid and exposed outdoor environments.

---

## 3. System Architecture & Hardware Stack

```
+-----------------------------------------------------------------------------+
|                          REMOTE SENSOR UNIT / MAST                          |
|  (Positioned for optimal microclimate measurement: canopy / clearing)       |
|                                                                             |
|  +-----------------------------------------------------------------------+  |
|  | Environmental Sensor Probes                                           |  |
|  | - Barometric Pressure, Temp, RH (Bosch BME280 / Industrial Probe)     |  |
|  | - Ambient Light / Solar Irradiance (TI OPT3001 / Pyranometer)         |  |
|  | - Remote Tipping-Bucket Rain Gauge / Pulse Counter                    |  |
|  +-----------------------------------+-----------------------------------+  |
|                                      |                                      |
|            [Differential I2C / RS-485 Modbus / SDI-12 / Shielded Cable]     |
|                                      |                                      |
+--------------------------------------+--------------------------------------+
                                       | (Distance: meters to 100m+)
                                       v
+-----------------------------------------------------------------------------+
|                MAIN CONTROLLER & TELEMETRY NODE (ON-SITE)                   |
|  (Positioned for optimal RF line-of-sight & solar harvesting)               |
|                                                                             |
|  +-----------------------------------------------------------------------+  |
|  | Power & Protection Subsystem                                          |  |
|  | - Switched Sensor Power Rail (Load switch to cut idle power)          |  |
|  | - TVS Diodes / Surge & ESD Line Protection                            |  |
|  | - Solar Harvesting (0.5W-2W) + LiFePO4 Battery                        |  |
|  +-----------------------------------+-----------------------------------+  |
|                                      |                                      |
|  +-----------------------------------v-----------------------------------+  |
|  | MCU & RF: STM32WLE5 (ARM Cortex-M4 + Sub-GHz LoRa Radio)              |  |
|  | - Remote Sensor Bus Drivers (UART/Modbus, Differential I2C, GPIO)     |  |
|  | - Data Acquisition, Filtering & CRC Validation                        |  |
|  | - On-Chip Edge Rain Prediction Algorithm (Primary Forecaster)        |  |
|  | - Local Non-Volatile Flash Ring-Buffer Logging                        |  |
|  | - Local Alert Indicator / Output (LED / Buzzer / Relay Trigger)       |  |
|  +-----------------------------------+-----------------------------------+  |
|                                      | LoRa (868/915/433 MHz)               |
+--------------------------------------+--------------------------------------+
                                       |
                                       v
+-----------------------------------------------------------------------------+
|              PRIMARY: ON-SITE ESTATE HUB / LOCAL GATEWAY                    |
|  - Local LoRaWAN Concentrator / Gateway (Estate Office / Base Station)      |
|  - Local Offline Dashboard & HMI (Local SQLite/InfluxDB + Web GUI / Screen) |
|  - Immediate Field Worker Alarm / Siren / WhatsApp or Local SMS Gateway     |
+--------------------------------------+--------------------------------------+
                                       | (Optional / Secondary Sync)
                                       v
+-----------------------------------------------------------------------------+
|              SECONDARY: OPTIONAL REMOTE CLOUD DASHBOARD                     |
|  - Long-term multi-estate historical analytics & telemetry archive          |
+-----------------------------------------------------------------------------+
```

### 3.1 Main Controller: STM32WLE5 SoC
- **Microcontroller**: STMicroelectronics **STM32WLE5** (ARM Cortex-M4 32-bit RISC core up to 48 MHz).
- **Integrated Radio**: Built-in Sub-GHz radio transceiver (LoRa / (G)FSK / MSK), eliminating the need for an external RF module.
- **Key Advantages**:
  - Single-die integration reduces BOM cost, PCB footprint, and failure points.
  - Ultra-low power modes (Stop, Standby, Shutdown down to sub-microamp levels).
  - Flexible peripherals (USART/UART with Modbus/RS-485 driver support, I2C, SPI, ADC, timers) for multi-sensor interfacing.

### 3.2 Connectivity & Remote Sensor Interfacing
The architecture physically decouples the **sensor probe assembly** (placed at canopy/clearing level) from the **MCU controller & antenna box** (placed at high-elevation/clear sky for maximum RF reach and solar irradiance):

1. **LoRa / LoRaWAN (Long-Range Node-to-Gateway)**:
   - Long transmission range (2–10+ km line-of-sight across plantation slopes).
   - Low energy consumption per packet transmission over license-free ISM bands.
2. **Sensor-to-MCU Field Bus Options (For Remote Sensor Heads)**:
   - **RS-485 / Modbus RTU (Recommended for long runs > 10m)**: Differential signaling immune to EMI, capable of runs up to 500m+ over inexpensive twisted-pair wire.
   - **SDI-12 (Serial Digital Interface at 1200 baud)**: Standard 3-wire agricultural bus for commercial environmental/soil probes over runs up to 60–100m.
   - **Differential I2C (e.g., PCA9615 / P82B715)**: Extends standard I2C over twisted-pair (Cat5/Cat6) up to 20–50m for direct BME280/OPT3001 sensor heads.
   - **Shielded Pulse / GPIO Loop**: Optical or reed-switch tipping bucket rain gauge with hardware RC filter and TVS diode clamp.
3. **Surge & Environmental Hardening**:
   - Outdoor cables in highland tea estates are prone to induced lightning surges; line protection using bidirectional TVS diodes (e.g., SM712 for RS-485) and common-mode chokes is required on external signal lines.

### 3.3 Sensor Suite (High-Quality Branded Sensors)
To ensure long-term stability, accuracy, and low drift in high-humidity plantation environments:
- **Barometric Pressure, Relative Humidity & Temperature**:
  - *Recommended*: **Bosch Sensortec BME280** (or **BME680** for additional air quality/gas sensing), integrated into a ventilated radiation shield / probe.
  - *Purpose*: Barometric pressure tendencies (rapid drops), vapor pressure, dew point calculation, and temperature inversions.
- **Ambient Light / Solar Radiation**:
  - *Recommended*: **Texas Instruments OPT3001** or photodiode-based pyranometer sensor.
  - *Purpose*: Detect sudden cloud build-up and solar attenuation associated with convective storms.
- **Ground Truth / Rain Validation**:
  - *Recommended*: Standard tipping-bucket rain gauge or optical precipitation sensor for recording actual rainfall volume and verifying predictions.
- **Auxiliary Sensors (Optional / Future Extensions)**:
  - Anemometer (wind speed and direction trends).
  - Soil moisture and leaf wetness sensors for agricultural monitoring.

---

## 4. Prediction Algorithm Strategy

The system prioritizes **simple, dependable, and explainable edge algorithms** that execute efficiently on the STM32WLE5 without requiring heavy compute or cloud dependencies.

### 4.1 Empirical Barometric & Microclimate Heuristics (e.g., Zambretti Forecaster)
- **Principle**: The classic Zambretti algorithm combined with modern meteorological trend heuristics.
- **Key Input Parameters**:
  - Absolute atmospheric pressure ($P$) and sea-level corrected pressure ($P_0$).
  - Pressure trend over 3-hour moving windows ($\Delta P / \Delta t$: rising, steady, or falling rapidly).
  - Rate of change in Relative Humidity ($\Delta RH / \Delta t$) and Temperature ($\Delta T / \Delta t$).
  - Calculated **Dew Point Depression** ($T - T_{dew}$): As $(T - T_{dew}) \to 0$, saturation approaches.
  - Solar radiation drop rate ($\Delta Lux / \Delta t$) during daylight hours.
- **Output States**:
  - Rain Unlikely / Fair Weather.
  - Rain Possible (Change Expected in 3–6 hrs).
  - Rain Imminent (High probability in 1–2 hrs / convective rain).

### 4.2 Lightweight Edge ML / Decision Trees (Optional Phase 2)
- Lightweight decision trees or logistic regression trained on historical plantation station data.
- Can be compiled to fixed-point C code or TinyML running within a few kilobytes of Flash/RAM on the Cortex-M4.

---

## 5. Power & Mechanical Considerations
- **Power Subsystem**:
  - Small monocrystalline solar panel (e.g., 0.5W–2W) + LiFePO4 or Li-ion battery with MPPT/solar charge management.
  - Aggressive duty-cycling: Node wakes every 5–15 minutes, samples sensors in tens of milliseconds, computes heuristics, sends LoRa telemetry, and returns to deep sleep.
- **Enclosure & Field Hardening**:
  - IP65/IP67 rated UV-resistant weatherproof enclosure.
  - Louvered solar radiation shield / Stevenson screen housing for temperature and humidity sensors to prevent direct sunlight bias.

---

## 6. Implementation Roadmap

| Phase | Milestone | Focus Areas |
| :--- | :--- | :--- |
| **Phase 1** | **Requirements & Hardware Selection** | Finalize BOM (STM32WLE5 development board, BME280/OPT3001, solar power supply). |
| **Phase 2** | **Algorithm Prototyping & Sensor Driver** | Develop sensor acquisition routines (I2C/SPI) and implement the barometric/humidity trend prediction algorithm in C. |
| **Phase 3** | **LoRa/LoRaWAN Communication** | Configure LoRaWAN stack (OTAA/ABP), payload formatting, gateway uplink, and downlink test. |
| **Phase 4** | **Power Optimization & Edge Testing** | Measure sleep currents, optimize sensor sample times, and test power autonomy. |
| **Phase 5** | **Field Deployment & Ground Truth Validation**| Deploy pilot node in the tea plantation, record ground truth rain data, calibrate heuristic thresholds. |
