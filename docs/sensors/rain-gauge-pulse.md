# Tipping-Bucket Rain Gauge & Pulse Counter Driver

## 1. Overview & Sensor Role

The **Tipping-Bucket Rain Gauge** serves as the **ground-truth validation instrument** and immediate precipitation recorder in the **Tea Plantation Rain Prediction System**.

### Primary Meteorological Functions
1. **Precipitation Accumulation**: Measures continuous rainfall rate ($\text{mm/hr}$) and cumulative daily volume ($\text{mm}$) across estate management zones.
2. **Ground-Truth Calibration**: Provides factual verification data to evaluate the accuracy of the on-chip predictive algorithms (Zambretti heuristics and pressure/humidity trend models).
3. **Rain-in-Progress State Latching**: Immediately transitions the node into the `RAIN_ACTIVE` telemetry state upon detecting the first physical bucket tip.

---

## 2. Mechanical Physics & Calibration Constant

### 2.1 Tipping Mechanism
The gauge funnel channels collected rainwater into a precision balanced dual-chamber seesaw bucket:
- **Calibrated Volume**: Each bucket chamber fills to exactly $0.20\text{ mm}$ (or $0.10\text{ mm}$ depending on funnel diameter) of equivalent precipitation before tipping.
- **Tip Event**: The seesaw swings, dumping the water through the drainage base and sweeping an internal neodymium magnet past a sealed magnetic reed switch.

$$\text{Rainfall Depth } (\text{mm}) = N_{\text{tips}} \times K_{\text{gauge}}$$

$$\text{Standard Metric Calibration: } K_{\text{gauge}} = 0.20\text{ mm per tip}$$

---

## 3. Hardware Interfacing & Debouncing Circuit

```mermaid
sequenceDiagram
    autonumber
    participant Bucket as Tipping-Bucket Seesaw
    participant Filter as Hardware RC Filter (10k + 100nF)
    participant EXTI as STM32WLE5 EXTI0 (PA0)
    participant Driver as rain_gauge_driver.c (ISR)
    participant State as Application State Machine

    Bucket->>Filter: Magnet passes Reed Switch (Contact closes to GND)
    Note over Filter: RC Low-Pass Filter attenuates sub-millisecond bounce
    Filter->>EXTI: Falling-edge trigger on PA0
    EXTI->>Driver: Trigger EXTI0_IRQHandler()
    
    alt Current Tick - Last Tick >= 50 ms (Valid Pulse)
        Driver->>Driver: Increment s_pulse_count
        Driver->>Driver: Update s_last_pulse_tick = Current Tick
        Driver->>State: Assert RAIN_ACTIVE flag
    else Current Tick - Last Tick < 50 ms (Mechanical Bounce)
        Driver->>Driver: Reject spurious trigger (debounced)
    end
```

### 3.1 Dual-Stage Noise Rejection
Mechanical reed switches exhibit contact chatter lasting $1\text{–}15\text{ ms}$ on impact:
1. **Stage 1 (Hardware RC Filter)**: A $10\,\text{k}\Omega$ pull-up resistor and $100\,\text{nF}$ ceramic capacitor filter out high-frequency contact bounce spikes ($\tau = 1.0\text{ ms}$).
2. **Stage 2 (Software ISR Lockout)**: A non-blocking $50\text{ ms}$ software window rejects any secondary triggers occurring within $50\text{ ms}$ of a valid tip.

---

## 4. Firmware Driver Architecture & Data Structures

Implemented in [`firmware/drivers/src/rain_gauge_driver.c`](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/firmware/drivers/src/rain_gauge_driver.c):

```c
#include "rain_gauge_driver.h"

#define RAIN_BUCKET_CALIB_MM        0.20f   /* mm of rain per tip */
#define RAIN_DEBOUNCE_MIN_MS        50      /* Minimum ms between valid tips */

static volatile uint32_t s_interval_tips = 0;
static volatile uint32_t s_hourly_tips = 0;
static volatile uint32_t s_daily_tips = 0;
static volatile uint32_t s_last_pulse_tick = 0;

/**
 * @brief EXTI0 Interrupt Service Routine (called from stm32wlxx_it.c).
 */
void rain_gauge_isr_handler(uint32_t current_tick_ms)
{
    /* Enforce 50 ms software lockout */
    if ((current_tick_ms - s_last_pulse_tick) >= RAIN_DEBOUNCE_MIN_MS) {
        s_interval_tips++;
        s_hourly_tips++;
        s_daily_tips++;
        s_last_pulse_tick = current_tick_ms;
    }
}

/**
 * @brief Reads and clears the interval rainfall accumulation.
 * @param[out] p_rain_mm Pointer to output float in mm.
 * @return Number of tips registered during the interval.
 */
uint32_t rain_gauge_read_interval(float *p_rain_mm)
{
    uint32_t tips;
    
    /* Atomic read-and-clear */
    __disable_irq();
    tips = s_interval_tips;
    s_interval_tips = 0;
    __enable_irq();

    if (p_rain_mm != NULL) {
        *p_rain_mm = ((float)tips) * RAIN_BUCKET_CALIB_MM;
    }
    return tips;
}
```

---

## 5. Ground Truth Verification & Model Performance Metrics

Rain gauge accumulation data is compared against earlier heuristic forecast predictions to generate formal confusion matrix scores:

```
                      Physical Rain Event (Ground Truth)
                       Rain Recorded         No Rain
Predicted    Rain     [True Positive (TP)]  [False Positive (FP)]
Forecast     No Rain  [False Negative (FN)] [True Negative (TN)]
```

### Key Performance Indices
1. **Probability of Detection ($\text{POD}$)**:
   $$\text{POD} = \frac{\text{TP}}{\text{TP} + \text{FN}} \ge 85\% \quad (\text{Target: Detect } >85\% \text{ of actual rain events})$$
2. **False Alarm Ratio ($\text{FAR}$)**:
   $$\text{FAR} = \frac{\text{FP}}{\text{TP} + \text{FP}} \le 20\% \quad (\text{Target: Less than } 20\% \text{ false alarms})$$
3. **Critical Success Index ($\text{CSI}$)**:
   $$\text{CSI} = \frac{\text{TP}}{\text{TP} + \text{FP} + \text{FN}}$$
