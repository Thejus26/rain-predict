# =============================================================================
# firmware/middleware/CMakeLists.txt
# Middleware & Services Layer (Ring Buffer, Filters, LoRaWAN, Power)
# =============================================================================

add_library(firmware_middleware STATIC
    src/ring_buffer.c
    src/moving_avg_filter.c
    src/dew_point.c
    src/power_mgr.c
    src/telemetry_codec.c
    src/flash_storage.c
    src/flash_playback.c
    src/lorawan_service.c
    src/lorawan_regional.c
    src/lorawan_tx_queue.c
)

target_include_directories(firmware_middleware PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/firmware/core/inc
    ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
)

target_link_libraries(firmware_middleware PUBLIC
    firmware_core
    firmware_drivers
)
