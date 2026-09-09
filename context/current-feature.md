# Current Feature

## Status

Phase 2 Complete (Sensors & Prediction Math Documentation)

## Goals

- [x] Create documentation roadmap (`docs/docs-roadmap.md`)
- [x] Document end-to-end system architecture (`docs/architecture/system-architecture.md`)
- [x] Document schematics, pinout & interfacing guide (`docs/hardware/schematics-and-pinout.md`)
- [x] Document bill of materials and component selection (`docs/hardware/bill-of-materials.md`)
- [x] Document power supply, solar sizing & energy budget (`docs/hardware/power-supply-and-solar.md`)
- [x] Document Bosch BME280 sensor integration (`docs/sensors/bme280-integration.md`)
- [x] Document TI OPT3001 solar irradiance & cloud detection (`docs/sensors/opt3001-solar-irradiance.md`)
- [x] Document tipping-bucket rain gauge & pulse counter (`docs/sensors/rain-gauge-pulse.md`)
- [x] Document Zambretti barometric heuristic algorithm (`docs/algorithms/zambretti-algorithm.md`)
- [x] Document meteorological formulas & dew point derivations (`docs/algorithms/meteorological-formulas.md`)
- [x] Document multi-variable trend detection & rain nowcasting model (`docs/algorithms/trend-detection-and-nowcasting.md`)

## Notes

- All technical diagrams implemented using standard Mermaid diagrams (`flowchart`, `sequenceDiagram`, `stateDiagram-v2`, `xychart-beta`).
- Sensor drivers and mathematical routines documented with Cortex-M4 single-precision float C implementations.
- Heuristic nowcasting scoring engine combines 3-hour pressure delta, humidity gradient, dew point depression, solar cloud drop, and Zambretti macro index.

## History

- 2026-09-09: Implemented Phase 1 documentation deliverables (System & Hardware Architecture).
- 2026-09-09: Implemented Phase 2 documentation deliverables (Sensors & Prediction Math).

