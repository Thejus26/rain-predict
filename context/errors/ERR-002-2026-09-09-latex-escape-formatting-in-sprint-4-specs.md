# Error Report: ERR-002 - LaTeX Formatting and Escape Errors in Sprint 4 Specs

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-002` |
| **Date & Time** | `2026-09-09 14:19:49 +0530` |
| **Commit SHA** | [`cc89f54`](https://github.com/Thejus26/rain-predict/commit/cc89f54f7aa8783a39b70ed822c7af9b464bddc9) |
| **Sprint / Task** | Sprint 4 Engineering Specifications |
| **Severity** | Low (Specification Documentation) |
| **Impacted Files** | [`context/specs/s4-t1.1-bme280-calibration-readout.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s4-t1.1-bme280-calibration-readout.md)<br>[`context/specs/s4-t2.1-opt3001-single-shot-readout.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s4-t2.1-opt3001-single-shot-readout.md)<br>[`context/specs/s4-t3.4-test-rain-gauge.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s4-t3.4-test-rain-gauge.md)<br>[`context/specs/s4-t4.2-modbus-crc16.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s4-t4.2-modbus-crc16.md) |

---

## 1. Description & Symptoms

Markdown linting and documentation build tools flagged unescaped LaTeX delimiter errors and broken math rendering across Sprint 4 sensor driver specifications (BME280 calibration register mappings, OPT3001 bitshift formulas, rain gauge pulse debounce limits, and Modbus CRC polynomial expressions).

---

## 2. Root Cause Analysis

Hexadecimal polynomial notations (e.g. `0xA001`), bitshift operations, and LaTeX math blocks contained unescaped underscores, brackets, and misaligned `$` delimiters that caused markdown table cell delimiters to break and MathJax/KaTeX renderers to throw parsing exceptions.

---

## 3. Resolution & Code Changes

Escaped polynomial notation, standardized math blocks with inline dollar signs (`$...$`), and corrected formula delimiters:

- Fixed Modbus CRC polynomial notation from unescaped raw text to code spans `` `0xA001` `` and proper math formulas.
- Escaped underscores and brackets in register address bitfields across BME280 and OPT3001 specification tables.

---

## 4. Verification & Prevention Guidelines

- Ensure hex register addresses (e.g. `0x88–0xA1`) and bitwise variables are wrapped in code spans rather than bare text in markdown tables.
- Validate LaTeX formulas using a markdown linter or KaTeX preview before committing.
