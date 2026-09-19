# =============================================================================
# firmware/app/CMakeLists.txt
# Application Layer (Zambretti Heuristics, Trend Detector, Rain Nowcaster)
# =============================================================================

add_library(firmware_app STATIC
    src/zambretti.c
    src/trend_detector.c
    src/rain_algo.c
    src/measurement_scheduler.c
    src/alert_manager.c
)

target_include_directories(firmware_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/firmware/core/inc
    ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
    ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
)

target_link_libraries(firmware_app PUBLIC
    $<$<TARGET_EXISTS:firmware_middleware>:firmware_middleware>
    $<$<TARGET_EXISTS:firmware_drivers>:firmware_drivers>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)
