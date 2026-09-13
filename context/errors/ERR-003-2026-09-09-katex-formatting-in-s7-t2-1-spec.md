# Error Report: ERR-003 - KaTeX Formatting Errors in S7-T2.1 Power Profiling Specification

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-003` |
| **Date & Time** | `2026-09-09 20:59:06 +0530` |
| **Commit SHA** | [`914e979`](https://github.com/Thejus26/rain-predict/commit/914e979d96ce059e7cf3b92a253584826bac006c) |
| **Sprint / Task** | Sprint 7 Specification (S7-T2.1 Power Profiling) |
| **Severity** | Low (Specification Documentation) |
| **Impacted Files** | [`context/specs/s7-t2.1-stop2-active-power-profiling.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/specs/s7-t2.1-stop2-active-power-profiling.md) |

---

## 1. Description & Symptoms

KaTeX parser threw syntax errors when rendering unit symbols for microampere-hours (`\mu Ah`) and microamperes (`\mu A`) inside tables, headings, and embedded Python script docstrings in the S7-T2.1 Power Profiling specification.

---

## 2. Root Cause Analysis

LaTeX requires proper grouping and spacing for multi-letter unit symbols in math mode. Unbraced macros like `\mu Ah` resulted in KaTeX interpreting `Ah` as mathematical variables without appropriate spacing, and unescaped backslashes inside Python multiline f-strings generated invalid KaTeX escape sequences.

---

## 3. Resolution & Code Changes

Standardized all electrical unit representations across the document:
- Replaced `\mu Ah` with `\,\mu\text{Ah}`.
- Replaced `\mu A` with `\,\mu\text{A}`.
- Fixed Python string escaping to ensure generated markdown reports emit valid KaTeX blocks:

```diff
-| **Single-Cycle 15-min Charge** | **{summary.single_cycle_total_charge_uah:.6f} µAh** | $\\le 1.60\\text{{ \\mu Ah}}$ | {"✅ PASS" if summary.single_cycle_total_charge_uah <= 1.60 else "❌ FAIL"} |
+| **Single-Cycle 15-min Charge** | **{summary.single_cycle_total_charge_uah:.6f} µAh** | $\\le 1.60\\,\\mu\\text{{Ah}}$ | {"✅ PASS" if summary.single_cycle_total_charge_uah <= 1.60 else "❌ FAIL"} |
```

---

## 4. Verification & Prevention Guidelines

- Standardize embedded engineering units: `\,\mu\text{A}`, `\,\mu\text{Ah}`, `\text{ mA}`, `\text{ mJ}`, `\text{ ms}`, `\text{ hPa}`.
- In Python scripts generating Markdown artifacts, use raw strings `r"""` or double backslashes for all LaTeX commands.
