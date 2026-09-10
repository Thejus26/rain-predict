# =============================================================================
# Tea Plantation Rain Prediction System - Root Makefile
# =============================================================================
# High-level build and test automation wrapper for CMake, Unity, and Python.
# =============================================================================

.DEFAULT_GOAL := help
.PHONY: all test sim firmware coverage asan format check-format lint clean help

# -----------------------------------------------------------------------------
# Configuration & Directory Paths
# -----------------------------------------------------------------------------
BUILD_DIR        ?= build
BUILD_TARGET_DIR ?= build_target
BUILD_COV_DIR    ?= build_cov
BUILD_ASAN_DIR   ?= build_asan
COVERAGE_DIR     ?= coverage_html
BUILD_TYPE       ?= Debug

# Cross-platform tooling detection
ifeq ($(OS),Windows_NT)
    PYTHON ?= python
    RM     := powershell -Command Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
else
    PYTHON ?= python3
    RM     := rm -rf
endif

# Source file discovery for formatting and static analysis
C_SOURCES := $(shell find firmware tests -type f -name '*.c' 2>/dev/null || dir /S /B firmware\*.c tests\*.c 2>nul)
C_HEADERS := $(shell find firmware tests -type f -name '*.h' 2>/dev/null || dir /S /B firmware\*.h tests\*.h 2>nul)

# -----------------------------------------------------------------------------
# Host Build & Unit Testing
# -----------------------------------------------------------------------------
all: ## Build host test binaries and algorithmic modules (Debug)
	@cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@cmake --build $(BUILD_DIR) --parallel

test: all ## Build and execute all host unit tests with Unity
	@ctest --test-dir $(BUILD_DIR) --output-on-failure --verbose

# -----------------------------------------------------------------------------
# Python Microclimate Simulation
# -----------------------------------------------------------------------------
sim: ## Run synthetic tea plantation microclimate simulation
	@$(PYTHON) tools/simulation/simulate_plantation_weather.py --output data/simulated_weather.csv
	@echo "[SIM] Synthetic tea estate climate simulation complete. Exported to data/simulated_weather.csv"

# -----------------------------------------------------------------------------
# Target STM32WLE5 Embedded Firmware
# -----------------------------------------------------------------------------
firmware: ## Cross-compile STM32WLE5 target firmware with arm-none-eabi-gcc
	@cmake -B $(BUILD_TARGET_DIR) -S . \
		-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
		-DCMAKE_BUILD_TYPE=Release
	@cmake --build $(BUILD_TARGET_DIR) --parallel
	@echo "[FIRMWARE] STM32WLE5 firmware build complete."

# -----------------------------------------------------------------------------
# Code Quality, Sanitizers & Coverage
# -----------------------------------------------------------------------------
coverage: ## Generate gcov/lcov HTML code coverage report
	@cmake -B $(BUILD_COV_DIR) -S . -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_COV_DIR) --parallel
	@ctest --test-dir $(BUILD_COV_DIR) --output-on-failure
	@lcov --capture --directory $(BUILD_COV_DIR) --output-file $(BUILD_COV_DIR)/coverage.info --rc lcov_branch_coverage=1
	@lcov --remove $(BUILD_COV_DIR)/coverage.info '/usr/*' '*/tests/*' '*/mocks/*' --output-file $(BUILD_COV_DIR)/coverage_filtered.info
	@genhtml $(BUILD_COV_DIR)/coverage_filtered.info --output-directory $(COVERAGE_DIR) --branch-coverage
	@echo "[COVERAGE] Coverage report generated at $(COVERAGE_DIR)/index.html"

asan: ## Build and run host tests with AddressSanitizer and UBSan
	@cmake -B $(BUILD_ASAN_DIR) -S . -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
	@cmake --build $(BUILD_ASAN_DIR) --parallel
	@ctest --test-dir $(BUILD_ASAN_DIR) --output-on-failure
	@echo "[ASAN] AddressSanitizer and UBSan execution verified clean."

format: ## Format all C source and header files using clang-format
	@clang-format -i $(C_SOURCES) $(C_HEADERS)
	@echo "[FORMAT] Applied .clang-format to all source and header files."

check-format: ## Verify code formatting against .clang-format without writing
	@clang-format --dry-run --Werror $(C_SOURCES) $(C_HEADERS)
	@echo "[FORMAT] All source files conform to .clang-format."

lint: ## Run cppcheck static analysis on firmware source files
	@cppcheck --enable=all --suppress=missingIncludeSystem --inline-suppr --error-exitcode=1 \
		-I firmware/core/inc \
		-I firmware/drivers/inc \
		-I firmware/middleware/inc \
		-I firmware/app/inc \
		firmware/
	@echo "[LINT] Static analysis passed with zero issues."

# -----------------------------------------------------------------------------
# Clean & Housekeeping
# -----------------------------------------------------------------------------
clean: ## Remove all build output directories and temporary files
	@$(RM) $(BUILD_DIR) $(BUILD_TARGET_DIR) $(BUILD_COV_DIR) $(BUILD_ASAN_DIR) $(COVERAGE_DIR) data/simulated_weather.csv compile_commands.json
	@echo "[CLEAN] Cleaned all build artifacts and directories."

# -----------------------------------------------------------------------------
# Self-Documenting Help Menu
# -----------------------------------------------------------------------------
help: ## Display this help message
	@echo "============================================================================="
	@echo " Tea Plantation Rain Prediction System - Available Make Targets"
	@echo "============================================================================="
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}'
