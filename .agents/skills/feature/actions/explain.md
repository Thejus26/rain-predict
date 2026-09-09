# Explain Action

1. Read `context/current-feature.md` to understand what was implemented.
2. Run `git diff main --name-only` to get the list of modified and new files.
3. For each file created or modified:
   - Show the file path (`.c`, `.h`, `.ld`, Makefile/CMakeLists.txt, `.py`).
   - Give a 1-2 sentence explanation of what it does and what changed.
   - Highlight key functions, structs, hardware registers, or algorithmic patterns used.
4. Conclude with a brief overview of how the firmware layers and components interconnect.

## Output Format

## Files Changed

**path/to/driver_bme280.c** (new)
Brief explanation of what this driver implements, its I2C/SPI interface, and power state management.

**path/to/rain_algo.c** (modified)
What changed in the prediction heuristic or threshold math and why.

## How It All Connects

Brief summary of data flow from sensor sampling -> edge prediction -> LoRaWAN payload formatting / local alert trigger.