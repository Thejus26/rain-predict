# Error Report: ERR-001 - GitHub KaTeX Table Rendering Error

## Metadata

| Field | Value |
| :--- | :--- |
| **Error ID** | `ERR-001` |
| **Date & Time** | `2026-09-09 11:42:53 +0530` |
| **Commit SHA** | [`ad6662b`](https://github.com/Thejus26/rain-predict/commit/ad6662ba4389ca362379a7839a908a684206e2c7) |
| **Sprint / Task** | Documentation / Algorithm Validation Specs |
| **Severity** | Low (Documentation / UI Rendering) |
| **Impacted Files** | [`docs/algorithms/algorithm-validation-and-tuning.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/docs/algorithms/algorithm-validation-and-tuning.md) |

---

## 1. Description & Symptoms

When viewing the meteorological verification metrics table in [`algorithm-validation-and-tuning.md`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/docs/algorithms/algorithm-validation-and-tuning.md) on GitHub and KaTeX-enabled Markdown viewers, the table failed to render into an HTML table. Instead, raw unformatted Markdown source was displayed or caused KaTeX delimiter parse failures.

---

## 2. Root Cause Analysis

GitHub Flavored Markdown (GFM) table parsers and KaTeX renderers have parsing conflicts when nested LaTeX text formatting commands like `$\text{POD}$` and complex math delimiters `$\ge 85.0\%$` are placed inside table cell borders (`|`). Unescaped pipe characters and deep math blocks inside headers/columns cause the Markdown lexer to miscalculate column counts.

---

## 3. Resolution & Code Changes

Replaced nested LaTeX `\text{...}` labels and complex LaTeX comparison macros in table cells with plain Unicode symbols (`≥`, `≤`) and clean math blocks:

```diff
 | Metric Name | Mathematical Formula | Target Threshold | Physical Significance in Estate Operations |
 | :--- | :---: | :---: | :--- |
-| **Probability of Detection ($\text{POD}$)** | $\text{POD} = \frac{\text{TP}}{\text{TP} + \text{FN}}$ | **$\ge 85.0\%$** | Percentage of rain events correctly warned in advance. |
-| **False Alarm Ratio ($\text{FAR}$)** | $\text{FAR} = \frac{\text{FP}}{\text{TP} + \text{FP}}$ | **$\le 20.0\%$** | Percentage of issued alarms where no rain occurred. |
-| **Critical Success Index ($\text{CSI}$)** | $\text{CSI} = \frac{\text{TP}}{\text{TP} + \text{FP} + \text{FN}}$ | **$\ge 70.0\%$** | Overall balanced skill score across events. |
-| **Heidke Skill Score ($\text{HSS}$)** | $\text{HSS} = \frac{2(\text{TP}\cdot\text{TN} - \text{FP}\cdot\text{FN})}{(\text{TP}+\text{FN})(\text{FN}+\text{TN}) + (\text{TP}+\text{FP})(\text{FP}+\text{TN})}$ | **$\ge 0.65$** | Skill relative to random chance ($1.0 = \text{perfect}$). |
-| **Warning Lead Time ($\text{LT}$)** | $\text{LT} = t_{\text{rain\_start}} - t_{\text{alert\_issued}}$ | **$45\text{–}120\text{ min}$** | Time available for field management action. |
+| **Probability of Detection (POD)** | $\text{POD} = \frac{\text{TP}}{\text{TP} + \text{FN}}$ | **≥ 85.0%** | Percentage of rain events correctly warned in advance. |
+| **False Alarm Ratio (FAR)** | $\text{FAR} = \frac{\text{FP}}{\text{TP} + \text{FP}}$ | **≤ 20.0%** | Percentage of issued alarms where no rain occurred. |
+| **Critical Success Index (CSI)** | $\text{CSI} = \frac{\text{TP}}{\text{TP} + \text{FP} + \text{FN}}$ | **≥ 70.0%** | Overall balanced skill score across events. |
+| **Heidke Skill Score (HSS)** | $\text{HSS} = \frac{2(\text{TP}\cdot\text{TN} - \text{FP}\cdot\text{FN})}{(\text{TP}+\text{FN})(\text{FN}+\text{TN}) + (\text{TP}+\text{FP})(\text{FP}+\text{TN})}$ | **≥ 0.65** | Skill relative to random chance ($1.0 = \text{perfect}$). |
+| **Warning Lead Time (LT)** | $\text{LT} = t_{\text{rain}} - t_{\text{alert}}$ | **45–120 min** | Time available for field management action. |
```

---

## 4. Verification & Prevention Guidelines

- Avoid nesting LaTeX `\text{...}` inside markdown table column labels.
- Use native Unicode mathematical operators (`≥`, `≤`, `±`, `°C`) in markdown table cells where full LaTeX math mode is not strictly necessary.
