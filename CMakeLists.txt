# =============================================================================
# CMakeLists.txt - Root Build System Configuration
# Project: Tea Plantation Rain Prediction System Firmware
# Target: STM32WLE5CC (ARM Cortex-M4 @ 48 MHz) & Native Host Test Harness
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# -----------------------------------------------------------------------------
# Project Definition & Metadata
# -----------------------------------------------------------------------------
project(rain_predict
    VERSION 1.0.0
    DESCRIPTION "Tea Plantation Rain Prediction System Firmware"
    LANGUAGES C ASM
)

# Enforce ISO C99 Standard without GNU extensions
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Export compile_commands.json for clangd / IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Build Options
# -----------------------------------------------------------------------------
option(BUILD_TESTING             "Build host unit test runner and test suites" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as fatal errors (-Werror)" ON)
option(ENABLE_ASAN               "Enable AddressSanitizer and UBSan (Host Only)" OFF)
option(ENABLE_COVERAGE           "Enable code coverage profiling (Host Only)" OFF)
option(BUILD_TARGET_FIRMWARE     "Build STM32WLE5 embedded target firmware" OFF)

# Target / Host Mode Selection
if(CMAKE_CROSSCOMPILING)
    set(BUILD_TARGET_FIRMWARE ON)
    set(BUILD_TESTING OFF)
    message(STATUS "Configuring for Target: STM32WLE5 (ARM Cortex-M4)")
else()
    message(STATUS "Configuring for Host: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
endif()

# -----------------------------------------------------------------------------
# Compiler Diagnostics & Strict Flags
# -----------------------------------------------------------------------------
include(cmake/CompilerFlags.cmake)

# -----------------------------------------------------------------------------
# Target Firmware Build (STM32WLE5CC)
# -----------------------------------------------------------------------------
if(BUILD_TARGET_FIRMWARE)
    # Target MCU preprocessor definitions
    add_compile_definitions(
        STM32WLE5xx
        USE_HAL_DRIVER
    )

    # Architectural layer subdirectories (included conditionally when created)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/CMakeLists.txt")
        add_subdirectory(firmware/core)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/drivers/CMakeLists.txt")
        add_subdirectory(firmware/drivers)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/middleware/CMakeLists.txt")
        add_subdirectory(firmware/middleware)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/app/CMakeLists.txt")
        add_subdirectory(firmware/app)
    endif()

    # Linker script location
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/src/STM32WLE5XX_FLASH.ld)

    # Target executable
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/core/src/main.c")
        add_executable(${PROJECT_NAME}.elf
            firmware/core/src/main.c
        )

        target_link_libraries(${PROJECT_NAME}.elf PRIVATE
            $<$<TARGET_EXISTS:firmware_app>:firmware_app>
            $<$<TARGET_EXISTS:firmware_middleware>:firmware_middleware>
            $<$<TARGET_EXISTS:firmware_drivers>:firmware_drivers>
            $<$<TARGET_EXISTS:firmware_core>:firmware_core>
        )

        target_link_options(${PROJECT_NAME}.elf PRIVATE
            -T${LINKER_SCRIPT}
            -Wl,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map
            -Wl,--gc-sections
            --specs=nano.specs
            --specs=nosys.specs
        )

        # Generate .hex, .bin, and print memory footprint
        add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
            COMMENT "Building HEX/BIN and printing target memory footprint"
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# Host Testing Build
# -----------------------------------------------------------------------------
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    # Host-compilable algorithmic modules (pure math & middleware)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/middleware/CMakeLists.txt")
        add_subdirectory(firmware/middleware)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/app/CMakeLists.txt")
        add_subdirectory(firmware/app)
    endif()

    # Unit & integration test suites
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()
