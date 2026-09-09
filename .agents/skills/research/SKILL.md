---
name: research
description: Run a research task on sensors, hardware specs, communication protocols, or algorithms to generate documentation
argument-hint: <prompt-name>
---

## Task

Execute research task: `$ARGUMENTS`

---

### Instructions

1. If no argument is provided, return error: `"Usage: /research <prompt-name>"`
2. Look for research prompt file at `context/research/{$ARGUMENTS}.md`
3. If not found, return error: `"Prompt file not found at context/research/{$ARGUMENTS}.md"`
4. Read the research prompt file, which defines:
   - **Output**: Target documentation file (e.g., `docs/sensors/bme280-driver-spec.md` or `context/lorawan-payload-spec.md`)
   - **Research Scope**: Hardware datasheets, MCU peripherals, register maps, mathematical formulas, or field protocols to investigate
   - **Include**: Key parameters, power profiles, registers, error states, formulas, or timing constraints to document
   - **Sources**: Relevant datasheet references, source code, or standards
5. Execute the research:
   - Inspect sensor datasheets, register definitions, and peripheral reference manuals (STM32WLE5, BME280, OPT3001, RS-485 transceiver)
   - Search codebase for existing HAL/driver interfaces and data structures
   - Research meteorological formulas (Zambretti forecaster, Magnus formula for dew point, barometric tendency thresholds)
6. Write structured findings to the specified output location
7. Provide a concise summary of the findings

---

### Rules

- This command produces **DOCUMENTATION ONLY**
- Do NOT modify source code files
- Do NOT create branches or commits
- Output documents should be written to `docs/` or `context/`
- Maintain high technical precision for pinouts, registers, and timing constraints
