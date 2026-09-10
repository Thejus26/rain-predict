# Cloud-Native Dev Container for STM32WLE5 Rain Prediction System
# Base Image: Ubuntu 22.04 LTS (x86_64 / aarch64 multi-arch)
FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

# Prevent interactive dialogs during apt installs
ENV DEBIAN_FRONTEND=noninteractive

# Install native compilation tools, ARM Cortex-M4 cross-compiler, static analyzers, and utility packages
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    make \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    gdb-multiarch \
    clang-format \
    cppcheck \
    python3 \
    python3-pip \
    python3-venv \
    git \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install Python scientific packages for microclimate simulation and telemetry decoders
RUN pip3 install --no-cache-dir \
    numpy \
    pandas \
    matplotlib \
    scipy

ENV DEBIAN_FRONTEND=dialog
