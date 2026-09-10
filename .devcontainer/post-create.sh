#!/usr/bin/env bash
set -e

echo "=== Initializing Rain-Predict Cloud Environment ==="
echo "Host GCC:           $(gcc --version | head -n 1)"
echo "ARM GCC:            $(arm-none-eabi-gcc --version | head -n 1)"
echo "CMake:              $(cmake --version | head -n 1)"
echo "Python:             $(python3 --version)"
echo "Clang-Format:       $(clang-format --version)"
echo "==================================================="
echo "Environment ready for build and test."
