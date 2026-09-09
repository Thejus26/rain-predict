# Current Feature

## Status

Specification Phase - Sprint 5 In Progress (Sprint 1..4 Complete)

## Goals

- Author and maintain rigorous, traceable, embedded C99 task specifications for the Tea Plantation Rain Prediction System firmware running on the STM32WLE5 SoC.
- Define target hardware configurations, clock trees (MSI 48 MHz, LSE 32.768 kHz, HSE 32 MHz), STM32CubeWL HAL module whitelist, and bounded non-blocking I2C/UART bus drivers with lockup recovery.
- Specify switched sensor power rails ($20\text{ms}$ stabilization guard), board indicators (LEDs, buzzer, siren auto-cutoff), pre-sleep GPIO analog isolation, ultra-low power Stop 2 sleep mode ($< 3.0\,\mu\text{A}$), Independent Watchdog ($8.0\text{s}$ timeout), and factory-calibrated battery ADC telemetry.
- Specify robust sensor drivers: Bosch BME280 (forced-mode burst sampling, single-precision FPU math, saturation recovery), TI OPT3001 (exponential lux conversion, day/night hysteresis, cloud attenuation scoring), and Tipping-Bucket Rain Gauge ($50\text{ms}$ EXTI debounce, atomic multi-horizon registers, rain rate intensity math, IMD/WMO 7-tier classification).
- Specify remote field bus protocol drivers: RS-485 Modbus RTU master (FC03 Read Holding Registers, polynomial `0xA001` bitwise/LUT CRC-16, `PA1` transceiver direction guard timing), and SDI-12 1200-baud agricultural bus ($13\text{ms}$ break / $9\text{ms}$ mark timing, command formatting, non-allocating ASCII float token parsing).
- Ensure all specifications include complete Doxygen API blueprints, state/sequence diagrams, register calculations, defensive guards, and 10-point verification test matrices.

## Notes

- **MCU Hardware Target**: STMicroelectronics STM32WLE5CC (ARM Cortex-M4 @ 48 MHz, 64 KB SRAM, 256 KB Flash).
- **Embedded C Standards**: Strict C99, `<stdint.h>`, `<stdbool.h>`, zero dynamic memory allocation (`malloc`/`free` prohibited), 4-space indentation, deterministic bounded non-blocking timeouts.
- **Power Architecture**: High-side P-MOSFET sensor power gating (`PA4` / `VSENS_SW`), $20\text{ms}$ stabilization delay, gated battery divider (`PB1` / `PB0`), pre-sleep analog isolation to suppress parasitic ESD clamping leakage.
- **Sleep & Watchdog Management**: Stop 2 mode with RTC periodic wakeup timer ($2\text{--}15\text{ min}$), full 64 KB SRAM retention ($< 3.0\,\mu\text{A}$ standby), and Independent Watchdog (IWDG, $8.0\text{s}$ timeout via 32 kHz LSI) with debug Stop 2 counter freeze.
- **Battery & Solar Telemetry**: Factory $V_{\text{REFINT\_CAL}}$ (`0x1FFF75AA`) analog supply calibration, 8-segment $\text{LiFePO}_4$ piecewise State of Charge mapping, and solar harvesting status classification.
- **Sensor & Field Bus Drivers**: BME280 ($I2C$, $0\text{x76}$), OPT3001 ($I2C$, $0\text{x44}$), Tipping-Bucket (`PA0` / `EXTI0`, $0.20\text{ mm/tip}$), RS-485 Modbus RTU (`USART1`, `PA1` DE/RE, $9600\text{ baud}$ 8N1), SDI-12 (`LPUART1`, `PC2` DIR, $1200\text{ baud}$ 7E1).

## History

- 2026-09-09: Implemented Phase 1 documentation deliverables (System & Hardware Architecture).
- 2026-09-09: Implemented Phase 2 documentation deliverables (Sensors & Prediction Math).
- 2026-09-09: Implemented Phase 3 documentation deliverables (Firmware & Telemetry Protocols).
- 2026-09-09: Implemented Phase 4 documentation deliverables (Power, Hardening & Mechanical).
- 2026-09-09: Implemented Phase 5 documentation deliverables (Field Validation & Calibration).
- 2026-09-09: Completed full Technical Documentation Roadmap across all 5 architecture, hardware, sensor, and algorithm phases.
- 2026-09-09: Created and linked all Sprint 1 task specifications (S1-T1.1 through S1-T3.4) covering CMake, Makefile, .clang-format, Unity test harness, Mock I2C/UART/GPIO drivers, and Python microclimate simulator.
- 2026-09-09: Created and linked all 17 Sprint 2 task specifications (S2-T1.1 through S2-T5.3) covering static ring buffer, moving average filters, Magnus-Tetens vapor pressure & dew point depression, hypsometric sea-level barometric reduction, 26-state Zambretti forecaster with monsoon/wind weighting, multi-variable gradient trend detection, 5-variable composite precipitation scoring ($CPI$), and 4-tier alert classification with simulated convective storm verification.
- 2026-09-09: Created and linked all 11 Sprint 3 task specifications (S3-T1.1 through S3-T4.4) covering STM32WLE5 48-pin GPIO allocations, multi-source clock tree configuration, HAL module whitelist, bounded I2C1 master driver with 9-clock bus recovery, dual-port UART/SDI-12 driver with circular buffers, switched P-MOSFET sensor rails with $20\text{ms}$ RC stabilization, board indicators and siren relays, pre-sleep GPIO analog isolation, Stop 2 deep sleep manager with RTC wakeup, $8.0\text{s}$ Independent Watchdog (IWDG) supervision, and factory-calibrated battery ADC telemetry with $\text{LiFePO}_4$ State of Charge modeling.
- 2026-09-09: Created and linked all 18 Sprint 4 task specifications (S4-T1.1 through S4-T5.3) covering Bosch BME280 calibration readout, forced-mode burst sampling, hardware FPU compensation, and saturation recovery; TI OPT3001 single-shot conversion, exponential lux/irradiance math, and day/night cloud attenuation scoring; Tipping-Bucket Rain Gauge 50ms EXTI debounce, atomic pulse counters, multi-horizon FIFO registers, rain rate derivative math, and IMD/WMO intensity classification; RS-485 Modbus RTU FC03 frame generator, polynomial 0xA001 bitwise/LUT CRC-16, and PA1 transceiver direction guard timing; SDI-12 1200-baud break/mark timing, command formatting, and multi-parameter ASCII response token parsing; with comprehensive ThrowTheSwitch Unity test suites across all drivers.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T1.1 covering 12-byte periodic LoRaWAN binary telemetry packet serializer and deserializer (telemetry_codec) with big-endian packing, fixed-point quantization, defensive range clamping, and 10-point test matrix.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T1.2 covering 4-byte urgent storm alert packet serializer and deserializer (telemetry_encode_alert / telemetry_decode_alert) with sub-byte cause bitfields, signed pressure rate quantization, rain intensity classification, and 10-point test matrix.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T1.3 covering the complete Unity test suite for the telemetry codec (test_telemetry_codec.c) with 12-byte periodic and 4-byte alert hex vector assertions, sub-zero sign preservation, bitfield isolation, boundary clamping, and 175-point combinatorial round-trip precision verification.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T2.1 covering ChirpStack v3 & v4 JavaScript payload codec (chirpstack_codec.js) with FPort 1 periodic decoding, FPort 2 urgent alert decoding, Zambretti English translations, and FPort 10 downlink configuration encoding.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T2.2 covering The Things Network (TTN v3 / The Things Stack) JavaScript payload formatter (ttn_decoder.js) with decodeUplink, encodeDownlink, and decodeDownlink handlers.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T2.3 covering automated Node.js test suite (test_decoders.js) validating ChirpStack and TTN decoder parity across all 10 periodic, alert, and downlink truth vectors.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T3.1 covering STM32WLE5 on-chip Flash page erase and 64-bit double-word programming driver (flash_storage) targeting Pages 120..127 (16 KB NVM partition) with hardware bounds protection and host test emulation.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T3.2 covering STM32WLE5 wear-leveling circular record ring buffer (flash_storage) storing up to 896 16-byte offline telemetry records (> 9 days retention) across Pages 120..126 with fast O(N) boot-time pointer reconstruction and in-place status invalidation.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T3.3 covering LoRaWAN reconnect historical telemetry playback & re-transmission queue manager (flash_playback) with FPort 3 multi-record batching, DR0..DR5 dynamic sizing, 1% duty-cycle throttling, live/alert preemption, and confirmed ACK in-place Flash invalidation.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T3.4 covering the Flash storage and playback queue Unity unit test suite (test_flash_storage.c) with 24 test cases spanning low-level Flash HAL driver, circular ring buffer wear-leveling, and LoRaWAN reconnect historical playback queues.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T4.1 covering STM32WL monolithic Sub-GHz LoRaWAN Class A network service (lorawan_service) with OTAA activation, AES-128 session keys, 3-pin RF switch control (PC3..PC5), unconfirmed/confirmed uplinks, and FPort 10 downlink dispatching.
- 2026-09-09: Created and linked Sprint 5 task specification S5-T4.2 covering LoRaWAN regional sub-band selection (IN865/EU868/US915), 1% duty-cycle tracking with exact Time-on-Air math, and End-Device Adaptive Data Rate (ADR) fallback state machine (lorawan_regional).
- 2026-09-09: Created and linked Sprint 5 task specification S5-T4.3 covering LoRaWAN multi-tier priority transmission queue manager (lorawan_tx_queue) with Tier 0 urgent storm alert preemption, Tier 1 periodic deduplication, Tier 2 historical playback throttling, and confirmed retry handling.
