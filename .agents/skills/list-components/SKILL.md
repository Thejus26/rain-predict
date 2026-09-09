---
name: list-components
description: List project components, firmware modules, and drivers
argument-hint: [layer-or-subdirectory]
---

## Task

List all firmware modules, drivers, headers, and source files across the project layers:
- **Application Layer**: State machine, prediction algorithm (`rain_algo`), measurement scheduler.
- **Middleware Layer**: LoRaWAN protocol stack, power management manager, ring-buffer filters.
- **Driver / BSP Layer**: Sensor drivers (BME280, OPT3001, Rain Gauge GPIO, RS-485 Modbus).
- **HAL / Core Layer**: STM32CubeWL HAL/LL, clock configurations, interrupt vectors.
- **Tools & Test Suite**: Python simulation harnesses, host unit tests.

If a `[layer-or-subdirectory]` argument is provided via `$ARGUMENTS`, only list files within that specific layer or directory.

## Output Format

- Numbered list of files with relative paths.
- Brief one-line description of each file's responsibility.
- Architectural layer mapping.
- Summary count at the end.

If no components are found, output: "No firmware components found."
