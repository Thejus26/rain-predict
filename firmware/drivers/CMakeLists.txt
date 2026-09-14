# =============================================================================
# firmware/drivers/CMakeLists.txt
# Peripheral & Sensor Drivers Layer (I2C, UART, GPIO, BME280, OPT3001)
# =============================================================================

add_library(firmware_drivers STATIC
    src/i2c_bus.c
    src/uart_bus.c
    src/bsp_power_rails.c
    src/bsp_indicators.c
    src/bsp_adc.c
    src/bme280_driver.c
    src/opt3001_driver.c
    src/rain_gauge_driver.c
)

target_include_directories(firmware_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/firmware/core/inc
)

target_link_libraries(firmware_drivers PUBLIC
    $<$<TARGET_EXISTS:firmware_core>:firmware_core>
)
