# =============================================================================
# CompilerFlags.cmake - Strict Warning and Diagnostic Flags
# Project: Tea Plantation Rain Prediction System Firmware
# Target: STM32WLE5 (ARM Cortex-M4) & Host Simulation
# Standards: ISO C99, MISRA-C Safety Principles, Zero-Warning Tolerance
# =============================================================================

# Strict C compiler diagnostic flags
set(STRICT_C_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wstrict-prototypes
    -Wpointer-arith
    -Wundef
    -Wmissing-prototypes
    -Wredundant-decls
    -Wformat=2
    -Wwrite-strings
)

# Enforce zero-warning tolerance if enabled
if(ENABLE_WARNINGS_AS_ERRORS)
    list(APPEND STRICT_C_FLAGS -Werror)
endif()

# Apply strict flags to all C targets across the project
foreach(FLAG IN LISTS STRICT_C_FLAGS)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:${FLAG}>
    )
endforeach()

# AddressSanitizer and UndefinedBehaviorSanitizer (Host builds only)
if(ENABLE_ASAN AND NOT CMAKE_CROSSCOMPILING)
    message(STATUS "Enabling AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan)")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Code Coverage Instrumentation (Host builds only)
if(ENABLE_COVERAGE AND NOT CMAKE_CROSSCOMPILING)
    message(STATUS "Enabling Code Coverage Instrumentation (--coverage)")
    add_compile_options(--coverage -O0 -g3)
    add_link_options(--coverage)
endif()
