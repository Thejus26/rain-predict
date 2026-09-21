# Tipping-Bucket Rain Gauge Field Water Calibration & Maintenance Standard Operating Procedure (SOP)

## 1. Document Overview & Scope

The tipping-bucket rain gauge ($0.20\text{ mm/tip}$) serves as the definitive **physical ground-truth instrument** for the entire autonomous tea plantation rain prediction system. Validating weather forecasts, contingency skill scoring (POD, FAR, CSI), and agronomic plucking/spraying decisions requires that physical rainfall detection operates with high volumetric accuracy ($\le \pm 2.0\%$) and chamber symmetry ($\le 1\text{ tip}$ delta).

This Standard Operating Procedure (SOP) provides field technicians with:
1. Analytical geometric formulas and theoretical water tip volume baselines.
2. Step-by-step procedures for the 6-phase dynamic water calibration protocol.
3. Mechanical stop screw fine-tuning kinematics.
4. Non-volatile memory (NVM) Flash configuration and LoRaWAN downlink synchronization.
5. Bi-annual and annual field maintenance protocols for tea estate environments.

---

## 2. Volumetric Physics & Geometric Baselines

### 2.1 Funnel Catch Area and Theoretical Tip Volume
The tipping-bucket rain gauge captures precipitation through a precision knife-edge circular aperture of diameter $D$:

$$A_{\text{funnel}} = \frac{\pi \cdot D^2}{4} \quad [\text{mm}^2]$$

For the standard agricultural $D = 200.0\text{ mm}$ collector funnel:

$$A_{\text{funnel}} = \frac{\pi \times (200.0)^2}{4} = 31,415.9265\text{ mm}^2 = \mathbf{314.1593\text{ cm}^2}$$

For each $0.20\text{ mm}$ increment of precipitation depth ($d_{\text{tip}} = 0.20\text{ mm} = 0.020\text{ cm}$), the theoretical volume of pure water required to displace one bucket seesaw chamber is:

$$V_{\text{tip}} = A_{\text{funnel}} \times d_{\text{tip}} \times 10^{-3} = 314.1593\text{ cm}^2 \times 0.020\text{ cm} = \mathbf{6.2832\text{ mL}}\text{ (or grams pure water @ }20^\circ\text{C)}$$

#### Collector Geometry Reference Table
| Collector Aperture Diameter ($D$) | Funnel Catch Area ($A_{\text{funnel}}$) | Calibrated Depth ($d_{\text{tip}}$) | Nominal Tip Volume ($V_{\text{tip}}$) | Expected Tips per Liter ($1,000\text{ mL}$) |
| :---: | :---: | :---: | :---: | :---: |
| **$200.0\text{ mm}$ (Standard Agro)** | $\mathbf{314.16\text{ cm}^2}$ | $\mathbf{0.20\text{ mm}}$ | $\mathbf{6.2832\text{ mL}}$ ($\mathbf{6.2832\text{ g}}$) | **$159.15\text{ tips/L}$** |
| $200.0\text{ mm}$ (High Resolution) | $314.16\text{ cm}^2$ | $0.10\text{ mm}$ | $3.1416\text{ mL}$ ($3.1416\text{ g}$) | $318.31\text{ tips/L}$ |
| $160.0\text{ mm}$ (Compact Field) | $201.06\text{ cm}^2$ | $0.20\text{ mm}$ | $4.0212\text{ mL}$ ($4.0212\text{ g}$) | $248.68\text{ tips/L}$ |

---

### 2.2 Dynamic Water Dispensing & Error Formulation
In a volumetric field test, a certified total water volume $V_{\text{total}}$ ($500.0\text{ mL} \pm 1.0\text{ mL}$) is dispensed through a constant-head dripping nozzle over a controlled duration $t_{\text{dispense}}$ ($\text{minutes}$).

1. **Theoretical Expected Tips ($N_{\text{expected}}$)**:
   $$N_{\text{expected}} = \frac{V_{\text{total}}}{V_{\text{tip}}} = \frac{500.0\text{ mL}}{6.2832\text{ mL/tip}} = 79.577 \approx \mathbf{79.6\text{ tips}}$$

2. **Relative Calibration Error Percentage ($E_{\text{cal}}$)**:
   $$E_{\text{cal}} = \left(\frac{N_{\text{actual}} - N_{\text{expected}}}{N_{\text{expected}}}\right) \times 100\%$$

3. **Field-Calibrated Gauge Multiplier ($K_{\text{gauge}}$)**:
   $$K_{\text{gauge}} = 0.200 \times \left(\frac{N_{\text{expected}}}{N_{\text{actual}}}\right) = 0.200 \times \left(\frac{V_{\text{total}}}{N_{\text{actual}} \times V_{\text{tip}}}\right) \quad [\text{mm/tip}]$$

4. **Simulated Rainfall Rate Intensity ($I_{\text{sim}}$)**:
   $$I_{\text{sim}} = \left(\frac{V_{\text{total}}}{A_{\text{funnel}} \times 10^{-4}}\right) \times \left(\frac{60}{t_{\text{dispense}}}\right) = \left(\frac{500.0}{31.4159}\right) \times \left(\frac{60}{t_{\text{dispense}}}\right) \quad [\text{mm/hr}]$$
   - **Low-Rate Static Test** ($I_{\text{sim}} = 25.0\text{ mm/hr}$): $t_{\text{dispense}} = \mathbf{38.2\text{ minutes}}$ (or $250.0\text{ mL}$ over $19.1\text{ min}$).
   - **High-Rate Dynamic Test** ($I_{\text{sim}} = 100.0\text{ mm/hr}$): $t_{\text{dispense}} = \mathbf{9.55\text{ minutes}}$ for $500.0\text{ mL}$.

---

### 2.3 Dynamic Siphon Loss & Non-Linear Intensity Correction
During torrential monsoon downpours ($I > 60\text{ mm/hr}$), water continues pouring through the funnel orifice during the mechanical bucket transition motion ($t_{\text{flip}} \approx 150\text{--}250\text{ ms}$). The excess unmeasured water volume $\Delta V_{\text{loss}}$ causes uncompensated gauges to under-report rainfall by $3\%\text{ to }8\%$.

The firmware driver ([`firmware/drivers/src/rain_gauge_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c)) applies second-order dynamic intensity compensation:

$$d_{\text{compensated}}(I) = d_{\text{nominal}} \times \left(1.0 + \alpha_{\text{dynamic}} \cdot \frac{I}{100.0}\right)$$

Where $\alpha_{\text{dynamic}} \approx 0.035$ ($+3.5\%$ correction at $100\text{ mm/hr}$).

---

## 3. Mechanical Hardware & Stop Screw Calibration Kinematics

The dual-chamber tipping bucket utilizes a precision seesaw pivot with adjustable threaded stop screws located beneath each chamber:

```
                      Collector Funnel (200mm Dia)
                                 │
                                 ▼
                     ┌───────────────────────┐
                     │ Precision Siphon Tube │
                     └──────────┬────────────┘
                                │
               Left Chamber     │     Right Chamber
                ┌─────────┐     │     ┌─────────┐
                │         │     │     │         │
                └────┬────┘     ▼     └────┬────┘
                     │       /═════\       │
       Neodymium ──► │======[ Pivot ]======│
         Magnet      │       \═════/       │
                     ▼                     ▼
                ┌─────────┐           ┌─────────┐
                │Stop Scr1│           │Stop Scr2│ (M3 x 0.5mm Thread)
                └─────────┘           └─────────┘
```

### 3.1 Stop Screw Kinematics
- **Thread Specification**: Standard metric **$\text{M3} \times 0.5\text{ mm pitch}$**.
- **Linear Travel**: $1\text{ full turn } (360^\circ) = 0.50\text{ mm}$ vertical displacement.
- **Volumetric Shift**: $1\text{ full turn} \approx \mathbf{0.060\text{ mm/tip}}$ volumetric change ($\approx 30\%$ shift).
- **Fine Adjustment Rule**:
  - **$1/8\text{ Turn } (45^\circ) \approx \mathbf{0.0075\text{ mm/tip}}$ ($\approx 3.75\%$ adjustment)**.
  - **Clockwise (CW)**: Raises the stop screw $\implies$ Bucket tips earlier $\implies$ **Decreases volume per tip** (Increases tip count $N$).
  - **Counter-Clockwise (CCW)**: Lowers the stop screw $\implies$ Bucket tips later $\implies$ **Increases volume per tip** (Decreases tip count $N$).

---

## 4. Standard Operating Procedure (SOP): 6-Phase Field Guide

```mermaid
sequenceDiagram
    autonumber
    actor Tech as Field Technician
    participant Gauge as Rain Gauge Assembly (PA0)
    participant Rig as Calibration Drip Kit (500mL Cylinder)
    participant Console as Maintenance UART / LoRaWAN Node

    Note over Tech,Gauge: Phase 1: Mechanical Inspection & Leveling
    Tech->>Gauge: Remove funnel; inspect bucket chambers, clean debris & webs
    Tech->>Gauge: Check bubble level; adjust mast leveling thumb screws until centered (<= 0.5°)
    Tech->>Gauge: Verify Reed Switch distance (2.0 ± 0.5 mm) & Agate pivot free motion

    Note over Tech,Rig: Phase 2: Static Calibration Test (Low Flow: 25 mm/hr)
    Tech->>Console: Enter Calibration Mode: "CAL_RAIN_START" (Zeros pulse counters)
    Tech->>Rig: Measure exactly 500 mL water (±1 mL @ 20°C) into dispenser
    Tech->>Rig: Install 25 mm/hr drip nozzle; allow 500 mL to drain over ~15 minutes
    Rig-->>Gauge: Uniform water drips -> Seesaw tips alternating Left/Right
    Tech->>Console: Query result: "CAL_RAIN_STATUS" -> N_left=40, N_right=40 (Total: 80 tips)

    Note over Tech,Gauge: Phase 3: Chamber Symmetry & Mechanical Adjustment
    alt Tip Symmetry Discrepancy |N_left - N_right| > 1 tip
        Tech->>Gauge: Adjust individual chamber stop screw by 1/8 turn on asymmetric side
        Tech->>Rig: Repeat 250 mL balancing run until |N_left - N_right| <= 1
    end

    Note over Tech,Rig: Phase 4: Dynamic High-Flow Test (100 mm/hr)
    Tech->>Rig: Dispense 500 mL through high-flow nozzle over ~4 minutes
    Tech->>Console: Check dynamic tip count (Target: 78 to 81 tips -> Error <= ±2%)

    Note over Tech,Console: Phase 5: NVM Calibration Programming & Sign-Off
    Tech->>Console: Issue command: "CAL_RAIN_COMMIT --k-factor 0.201" (Flash Sector 7 commit)
    Console->>Console: Store uint16_t calib_factor_um = 201 µm in Flash NVM
    Tech->>Gauge: Re-install top funnel; apply silicone grease on sealing gasket
```

### 4.1 Phase 1: Mechanical Inspection & Leveling
1. **Collector Funnel Removal**: Twist counter-clockwise and lift the outer funnel housing to expose the internal seesaw assembly.
2. **Debris Clearing**:
   - Inspect the stainless steel mesh debris filter for tea flower fragments, dust, and spider webs. Clean with fresh water and a soft nylon brush.
   - Inspect baseplate drain ports to ensure water drains freely with zero pooling.
3. **Bearing & Magnet Inspection**:
   - Manually tip the bucket seesaw; ensure agate bearings rotate smoothly with zero mechanical stiction.
   - Verify the cylindrical neodymium magnet passes within $2.0 \pm 0.5\text{ mm}$ of the hermetic reed switch capsule.
   - Clean the Teflon/polycarbonate bucket chambers using an alcohol wipe. Avoid abrasive materials that degrade surface hydrophobicity.
4. **Precision Spirit Bubble Leveling**:
   - Inspect the circular bubble level mounted on the gauge casting.
   - Adjust the three mast mounting thumb screws until the air bubble is **dead-centered within the inner ring** ($\le 0.5^\circ$ inclination).

### 4.2 Phase 2: Calibration Tooling Setup
Assemble the field calibration kit:
- **Graduated Cylinder**: $500.0\text{ mL}$ Class-A volumetric cylinder ($\pm 1.0\text{ mL}$ at $20^\circ\text{C}$).
- **Constant-Head Drip Bottle**: Mariotte siphon container with calibrated dripping nozzles ($25\text{ mm/hr}$ and $100\text{ mm/hr}$).
- **Field Terminal Console**: USB-UART interface on `PA2`/`PA3` ($115,200\text{ baud}$, 8N1) or LoRaWAN field programmer.

### 4.3 Phase 3: Low-Rate Static Calibration Test ($25\text{ mm/hr}$)
1. Place the firmware driver into Calibration Mode:
   ```bash
   $ CAL_RAIN_START
   [RAIN_CAL] Calibration Mode Active. Pulse counters zeroed. EXTI0 debouncing active.
   ```
2. Measure exactly **$500.0\text{ mL}$** of clean water into the dispenser.
3. Mount the dispenser over the rain gauge funnel with the **$25\text{ mm/hr}$ nozzle**.
4. Allow water to discharge steadily over **$15\text{ to }20\text{ minutes}$**.
5. Stop the test and retrieve chamber tip counts:
   ```bash
   $ CAL_RAIN_STATUS
   [RAIN_CAL] Total Tips: 80 | Left Bucket: 40 | Right Bucket: 40
   [RAIN_CAL] Expected Tips: 79.58 | Relative Error: +0.53% [PASS <= 2.0%]
   [RAIN_CAL] Multiplier: K_gauge = 0.1989 mm/tip (198 um/tip)
   ```

### 4.4 Phase 4: Chamber Symmetry Balancing
If the difference between left and right chamber tips exceeds $1\text{ tip}$ ($|N_{\text{left}} - N_{\text{right}}| > 1$):
1. Identify the bucket chamber recording **fewer tips** (tipping late $\implies$ requires too much water).
2. Loosen the $\text{M3}$ brass locknut on that chamber's stop screw.
3. Turn the stop screw **Clockwise (CW) by $1/8\text{ turn}$ ($45^\circ$)** to raise the stop, tipping earlier.
4. Tighten the locknut and re-run a $250\text{ mL}$ balance run until:
   $$|N_{\text{left}} - N_{\text{right}}| \le 1\text{ tip}$$

### 4.5 Phase 5: High-Rate Dynamic Siphon Calibration ($100\text{ mm/hr}$)
1. Fill the dispenser with **$500.0\text{ mL}$** water.
2. Attach the **$100\text{ mm/hr}$ high-flow nozzle** and dispense over **$4\text{ to }5\text{ minutes}$**.
3. Confirm that total recorded tips satisfy the dynamic acceptance gate:
   $$N_{\text{actual, high}} \in [78, 81]\text{ tips} \quad (|E_{\text{cal}}| \le \pm 2.0\%)$$

### 4.6 Phase 6: NVM Calibration Programming & Field Sign-Off
1. Commit the calibrated $K_{\text{gauge}}$ factor into **STM32WLE5 Flash Sector 7 (`0x0803F800`)**:
   ```bash
   $ CAL_RAIN_COMMIT --k-factor 0.199 --tech-id 8402
   [NVM_CFG] Erasing Flash Sector 7 (0x0803F800)... OK
   [NVM_CFG] Writing rain gauge calibration: K_gauge = 199 um/tip (CRC: 0x5C82)... OK
   [NVM_CFG] Flash verification verified successfully.
   ```
2. Re-install the collector funnel housing. Ensure the silicone O-ring is lubricated and seated firmly to prevent insect ingress.
3. Verify that the node transmits a confirmed LoRaWAN telemetry uplink on FPort 1 with zero false rain pulses after reassembly.

---

## 5. Non-Volatile Memory (NVM) Flash Storage Layout

The rain gauge calibration block resides in Flash Sector 7 in a 6-byte packed layout:

```c
/**
 * @brief Rain Gauge Non-Volatile Calibration Parameters.
 * Resides within the 32-byte System Configuration Block in Flash Sector 7.
 */
typedef struct __attribute__((packed)) {
    uint16_t calib_factor_um;    /* Rain depth per tip in micro-meters (e.g. 199 um = 0.199 mm) */
    uint16_t dynamic_coeff_ppm;  /* Dynamic flow correction in PPM per mm/hr (e.g. 350 = +0.035%) */
    uint8_t  funnel_diameter_mm; /* Collector funnel diameter in mm (e.g. 200 mm) */
    uint8_t  debounce_lockout_ms;/* Software debounce window in ms (e.g. 50 ms) */
} rg_nvm_config_t;
```

---

## 6. Remote LoRaWAN & Local Maintenance Commands

### 6.1 LoRaWAN Downlink Protocol (FPort 10, Command `0x05`)
To re-calibrate an operating node remotely over LoRaWAN:
- **Command Byte**: `0x05` (Set Rain Gauge K-Factor).
- **Payload Bytes (3 Bytes total)**:
  ```
  Byte 0: Command ID = 0x05
  Byte 1–2: K_gauge in micrometers (uint16_t Big-Endian) -> e.g. 201 um = 0x00C9
  ```
- **Firmware Clamping**: $K_{\text{gauge}}$ is clamped within $[150, 250]\,\mu\text{m/tip}$ ($0.150\text{ to }0.250\text{ mm/tip}$). Out-of-bounds frames are rejected with `STATUS_ERR_OUT_OF_RANGE`.

### 6.2 Maintenance UART Shell Command Summary
| Command String | Parameters | Functional Description |
| :--- | :--- | :--- |
| `CAL_RAIN_START` | None | Enters calibration mode, zeroes accumulators, enables $50\text{ms}$ EXTI0 debouncing. |
| `CAL_RAIN_STATUS` | None | Returns $N_{\text{left}}$, $N_{\text{right}}$, $N_{\text{total}}$, calculated error %, and balance state. |
| `CAL_RAIN_COMMIT` | `--k-factor <mm>` `--tech-id <id>` | Commits calibrated multiplier into Flash Sector 7 with CRC-16. |
| `CAL_RAIN_RESET` | None | Restores default factory calibration factor ($0.200\text{ mm/tip} = 200\,\mu\text{m}$). |

---

## 7. Preventive Maintenance & Estate Service Schedule

| Service Interval | Environmental Trigger | Inspection & Maintenance Checklist Items |
| :---: | :--- | :--- |
| **Monthly** | Routine plucking cycle | Visually inspect outer funnel rim; remove leaves or bird droppings. |
| **Bi-Annual** | Pre-Monsoon (May) & Post-Monsoon (October) | - Remove funnel; clean internal mesh and siphon tube with clean water.<br>- Check spirit bubble level ($< 0.5^\circ$).<br>- Run $500\text{ mL}$ static calibration check ($N \in [78, 81]$ tips). |
| **Annual** | Pre-Spring Dry Period (March) | - Full two-rate calibration test ($25\text{ mm/hr}$ and $100\text{ mm/hr}$).<br>- Inspect agate bearings for wear; clean pivot pins.<br>- Check reed switch glass capsule for cracks or oxidation.<br>- Re-program Flash NVM calibration trim if $|E_{\text{cal}}| > 2.0\%$. |
