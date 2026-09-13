# =============================================================================
# firmware/core/CMakeLists.txt
# Core MCU Architecture, Board Configuration & Hardware Abstraction Layer
# =============================================================================

add_library(firmware_core STATIC
    src/board_config.c
)

target_include_directories(firmware_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)
