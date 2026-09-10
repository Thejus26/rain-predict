# Cloud-Native Build & Development Plan (GitHub Codespaces)

## 1. Overview & Objectives

This document establishes the zero-local-install cloud development workflow for the **Tea Plantation Rain Prediction System (`rain-predict`)**. 

Using **GitHub Codespaces** paired with **VS Code Dev Containers**, developers can write, format, cross-compile, host-simulate, and unit-test the entire embedded C firmware and Python weather modeling suite purely within a web browser or remote container—without installing toolchains or consuming resources on a local laptop.

```
+---------------------------------------------------------------------------------------+
|                               GITHUB CODESPACES (CLOUD)                               |
|                                                                                       |
|  +---------------------------------------------------------------------------------+  |
|  | VS Code Web IDE / Browser Interface                                            |  |
|  | Extensions: C/C++, CMake Tools, Cortex-Debug, Python, Clang-Format               |  |
|  +---------------------------------------+-----------------------------------------+  |
|                                          |                                            |
|  +---------------------------------------v-----------------------------------------+  |
|  | Isolated Docker Dev Container (Ubuntu 22.04 LTS Base)                           |  |
|  |                                                                                 |  |
|  |  +------------------------------+     +--------------------------------------+  |  |
|  |  | ARM Embedded Toolchain       |     | Host Native Toolchain                |  |  |
|  |  | - gcc-arm-none-eabi          |     | - gcc / g++ (x86_64)                 |  |  |
|  |  | - libnewlib-arm-none-eabi    |     | - cmake (>= 3.22), ninja, make       |  |  |
|  |  | - gdb-multiarch              |     | - clang-format, cppcheck             |  |  |
|  |  | (Builds STM32WLE5 .elf/.bin) |     | (Builds & runs Unity unit tests)     |  |  |
|  |  +------------------------------+     +--------------------------------------+  |  |
|  |                                                                                 |  |
|  |  +---------------------------------------------------------------------------+  |  |
|  |  | Python Microclimate Simulator & Analysis Suite                            |  |  |
|  |  | - Python 3.10+, numpy, pandas, matplotlib, scipy                          |  |  |
|  |  | (Generates synthetic tea estate weather curves & validates nowcasting)    |  |  |
|  |  +---------------------------------------------------------------------------+  |  |
|  +---------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------+
```

---

## 2. Implementation Plan

### Phase 1: Dev Container Infrastructure (`.devcontainer/`)
Create the container configuration files to standardize the cloud development environment.

1. **`.devcontainer/Dockerfile`**:
   - Base image: `mcr.microsoft.com/devcontainers/base:ubuntu-22.04`
   - Install native build essentials (`build-essential`, `cmake`, `ninja-build`, `make`).
   - Install ARM cross-compilation toolchain (`gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `gdb-multiarch`).
   - Install static analysis and linting tools (`clang-format`, `cppcheck`).
   - Install Python 3 scientific stack (`numpy`, `scipy`, `pandas`, `matplotlib`).

2. **`.devcontainer/devcontainer.json`**:
   - Link to `Dockerfile`.
   - Pre-install recommended VS Code extensions:
     - `ms-vscode.cpptools` (C/C++ IntelliSense, debugging)
     - `ms-vscode.cmake-tools` (CMake target management)
     - `marus25.cortex-debug` (ARM Cortex debugging support)
     - `ms-python.python` (Python language server)
     - `xaver.clang-format` (Auto code formatting on save)
   - Configure workspace settings: standard formatting on save, default include paths for CMSIS/HAL, and CMake build presets.

---

### Phase 2: Dual-Target Build System Setup

The build system will support two distinct compilation targets:

1. **Host Target (`gcc` / `x86_64`)**:
   - Compiles pure C business logic, moving average filters, Magnus-Tetens dew point algorithms, Zambretti heuristics, and telemetry encoders against mock I2C/UART/GPIO drivers.
   - Runs ThrowTheSwitch Unity test suites rapidly in the cloud terminal (`ctest`).
2. **Firmware Target (`arm-none-eabi-gcc` / Cortex-M4)**:
   - Cross-compiles the target firmware with STM32CubeWL HAL, startup assembly, and linker scripts (`STM32WLE5XX_FLASH.ld`).
   - Produces deployable release binaries (`firmware.elf`, `firmware.bin`, `firmware.hex`).

---

### Phase 3: Development & Simulation Workflows

#### 1. Host-Based Unit Testing & Mock Simulation
Developers can test all algorithmic components without physical STM32 hardware:
```bash
# Configure and build all host unit tests
cmake -B build-host -S . -DTARGET_PLATFORM=HOST
cmake --build build-host

# Run the full Unity test suite
ctest --test-dir build-host --output-on-failure
```

#### 2. Synthetic Microclimate Weather Simulation
Developers can run and visualize realistic plantation weather scenarios (barometric pressure drops, rapid humidity increases, solar irradiance attenuation):
```bash
# Run 30-day convective storm and diurnal weather simulation
python3 tools/simulation/simulate_plantation_weather.py --scenario convective_storm

# Test prediction accuracy and nowcaster sensitivity
python3 tools/simulation/test_forecast_accuracy.py
```

#### 3. Cross-Compiling Target Firmware
```bash
# Configure and cross-compile ARM Cortex-M4 firmware
cmake -B build-target -S . -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake
cmake --build build-target
```

---

### Phase 4: CI/CD & Automated Cloud Verification

Implement a **GitHub Actions** workflow (`.github/workflows/build-and-test.yml`) to automatically execute on every push and pull request:
1. **Code Hygiene**: Run `clang-format` and `cppcheck` across `firmware/` and `tests/`.
2. **Host Unit Tests**: Build and run all Unity test suites.
3. **Synthetic Validation**: Execute Python weather regression scenarios.
4. **Target Cross-Compilation**: Build ARM firmware binary and archive `firmware.bin` as a downloadable artifact.

---

## 3. Daily Developer Quick-Start Guide

### Starting Development in GitHub Codespaces
1. Push changes to the GitHub repository.
2. In the GitHub repo UI, click **`< > Code`** -> **Codespaces** tab -> **`Create codespace on main`**.
3. In under 60 seconds, the browser will open a fully configured VS Code environment with all compilers, linters, and libraries pre-installed.

### Quota Management & Best Practices
* **Free Quota**: GitHub provides **60 free core-hours per month** for personal accounts.
* **Auto-Stop**: The container is configured to auto-stop after 15 minutes of inactivity to preserve free quota.
* **Manual Stop**: When finished working, press `F1` -> **Codespaces: Stop Current Codespace**.

---

## 4. Summary of Deliverables

| Deliverable | File Path | Purpose |
| :--- | :--- | :--- |
| **Dev Container Definition** | `.devcontainer/Dockerfile` | Ubuntu 22.04 environment with ARM GCC, CMake, Python |
| **VS Code Config** | `.devcontainer/devcontainer.json` | Extension bindings and editor settings |
| **Toolchain File** | `cmake/arm-none-eabi-toolchain.cmake` | CMake rules for cross-compiling STM32WLE5 Cortex-M4 |
| **Root Build File** | `CMakeLists.txt` | Dual-target configuration (Host Test vs Target Firmware) |
| **Unified Makefile** | `Makefile` | One-touch shortcuts (`make test`, `make firmware`, `make sim`) |
| **CI/CD Pipeline** | `.github/workflows/ci.yml` | Automated GitHub Actions verification and artifact generation |
