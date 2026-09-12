# Explain Action

1. Read `context/current-feature.md` to understand what was implemented.
2. Run `git diff main --name-only` (or base branch) to get the list of modified and new files.
3. For each file created or modified:
   - Show the file path (`.c`, `.h`, `.ld`, Makefile/CMakeLists.txt, `.py`).
   - Give a clear 1-2 sentence plain-English explanation of what it does and why it was changed, avoiding unnecessary dense jargon.
   - Highlight key functions, structs, hardware registers, or algorithmic concepts in simple, accessible terms.
4. Conclude with a simple, intuitive overview of how everything connects together from sensor readings to decision-making and alerts.

## Output Format

## Files Changed

**path/to/driver_bme280.c** (new)
Simple, clear explanation of what this driver does, how it talks to the sensor, and how it saves power.

**path/to/rain_algo.c** (modified)
What changed in the prediction logic in simple terms and what practical problem it solves.

## How It All Connects

Simple, step-by-step summary of the journey from raw sensor data -> intelligent edge calculations -> actionable alarms & wireless telemetry.