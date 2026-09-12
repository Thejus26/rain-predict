# =============================================================================
# firmware/middleware/CMakeLists.txt
# Middleware & Services Layer (Ring Buffer, Filters, LoRaWAN, Power)
# =============================================================================

add_library(firmware_middleware STATIC
    src/ring_buffer.c
    src/moving_avg_filter.c
)

target_include_directories(firmware_middleware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/firmware/core/inc
)
