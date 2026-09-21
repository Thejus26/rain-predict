# =============================================================================
# Tea Plantation Rain Prediction System - Host Unit Testing Build
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# Enforce include path visibility
set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)
set(MOCKS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mocks)

# -----------------------------------------------------------------------------
# 1. Unity Framework Static Library Target
# -----------------------------------------------------------------------------
add_library(unity STATIC
    ${UNITY_DIR}/unity.c
)

target_include_directories(unity PUBLIC
    ${UNITY_DIR}
)

target_compile_definitions(unity PUBLIC
    UNITY_INCLUDE_CONFIG_H
)

# Apply strict warning flags except for third-party macro pedantics
target_compile_options(unity PRIVATE
    -Wall
    -Wextra
)

# -----------------------------------------------------------------------------
# 2. Mock Hardware Layer Static Library Target
# -----------------------------------------------------------------------------
file(GLOB MOCK_SOURCES "${MOCKS_DIR}/*.c")
if(MOCK_SOURCES)
    add_library(test_mocks STATIC
        ${MOCK_SOURCES}
    )

    target_include_directories(test_mocks PUBLIC
        ${MOCKS_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_link_libraries(test_mocks PUBLIC
        unity
        $<$<TARGET_EXISTS:firmware_drivers>:firmware_drivers>
    )
endif()

# -----------------------------------------------------------------------------
# 3. Standard Helper Function: add_firmware_test
# -----------------------------------------------------------------------------
function(add_firmware_test TEST_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES LIBRARIES INCLUDES)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(EXEC_NAME test_${TEST_NAME})

    add_executable(${EXEC_NAME}
        ${TEST_SOURCES}
    )

    target_include_directories(${EXEC_NAME} PRIVATE
        ${UNITY_DIR}
        ${MOCKS_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
        ${TEST_INCLUDES}
    )

    target_link_libraries(${EXEC_NAME} PRIVATE
        unity
        $<$<TARGET_EXISTS:test_mocks>:test_mocks>
        $<$<TARGET_EXISTS:firmware_drivers>:firmware_drivers>
        $<$<TARGET_EXISTS:firmware_core>:firmware_core>
        $<$<TARGET_EXISTS:firmware_middleware>:firmware_middleware>
        $<$<TARGET_EXISTS:firmware_app>:firmware_app>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
        ${TEST_LIBRARIES}
    )

    # Register test with CTest test runner
    add_test(NAME ${TEST_NAME} COMMAND ${EXEC_NAME})

    # Set custom test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endfunction()

# -----------------------------------------------------------------------------
# 4. Mock Verification & Sanity Tests
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_sanity.c")
    add_firmware_test(sanity
        SOURCES unit/test_sanity.c
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_mock_i2c.c")
    add_firmware_test(mock_i2c
        SOURCES unit/test_mock_i2c.c
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_mock_uart.c")
    add_firmware_test(mock_uart
        SOURCES unit/test_mock_uart.c
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_mock_gpio.c")
    add_firmware_test(mock_gpio
        SOURCES unit/test_mock_gpio.c
    )
endif()

# -----------------------------------------------------------------------------
# 5. Middleware Data Structures & Filtering Tests
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_ring_buffer.c")
    add_firmware_test(ring_buffer
        SOURCES unit/test_ring_buffer.c
        LIBRARIES firmware_middleware
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_moving_avg.c")
    add_firmware_test(moving_avg
        SOURCES unit/test_moving_avg.c
        LIBRARIES firmware_middleware
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_dew_point.c")
    add_firmware_test(dew_point
        SOURCES unit/test_dew_point.c
        LIBRARIES firmware_middleware
    )
endif()

# -----------------------------------------------------------------------------
# 6. Application Algorithms & Nowcasting Tests
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_zambretti.c")
    add_firmware_test(zambretti
        SOURCES unit/test_zambretti.c
        LIBRARIES firmware_app firmware_middleware
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_trend_detector.c")
    add_firmware_test(trend_detector
        SOURCES unit/test_trend_detector.c
        LIBRARIES firmware_app firmware_middleware
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_rain_algo.c")
    add_firmware_test(rain_algo
        SOURCES unit/test_rain_algo.c
        LIBRARIES firmware_app firmware_middleware
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_measurement_scheduler.c")
    add_firmware_test(measurement_scheduler
        SOURCES unit/test_measurement_scheduler.c
        LIBRARIES firmware_app firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_alert_manager.c")
    add_firmware_test(alert_manager
        SOURCES unit/test_alert_manager.c
        LIBRARIES firmware_app firmware_drivers firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_app_fault_handler.c")
    add_firmware_test(app_fault_handler
        SOURCES unit/test_app_fault_handler.c
        LIBRARIES firmware_app firmware_drivers firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_app_state_machine.c")
    add_firmware_test(app_state_machine
        SOURCES unit/test_app_state_machine.c
        LIBRARIES firmware_app firmware_drivers firmware_middleware firmware_core
    )
endif()

# -----------------------------------------------------------------------------
# 7. Core MCU Architecture & BSP Tests
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_board_config.c")
    add_firmware_test(board_config
        SOURCES unit/test_board_config.c
        LIBRARIES firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_system_clock.c")
    add_firmware_test(system_clock
        SOURCES unit/test_system_clock.c
        LIBRARIES firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_hal_conf.c")
    add_firmware_test(hal_conf
        SOURCES unit/test_hal_conf.c
        LIBRARIES firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_i2c_bus.c")
    add_firmware_test(i2c_bus
        SOURCES unit/test_i2c_bus.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_uart_bus.c")
    add_firmware_test(uart_bus
        SOURCES unit/test_uart_bus.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_bsp_power_rails.c")
    add_firmware_test(bsp_power_rails
        SOURCES unit/test_bsp_power_rails.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_bsp_indicators.c")
    add_firmware_test(bsp_indicators
        SOURCES unit/test_bsp_indicators.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_power_mgr.c")
    add_firmware_test(power_mgr
        SOURCES unit/test_power_mgr.c
        LIBRARIES firmware_middleware firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_watchdog.c")
    add_firmware_test(watchdog
        SOURCES unit/test_watchdog.c
        LIBRARIES firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_bsp_adc.c")
    add_firmware_test(bsp_adc
        SOURCES unit/test_bsp_adc.c
        LIBRARIES firmware_middleware firmware_drivers firmware_core
    )
endif()

# -----------------------------------------------------------------------------
# 8. Sensor Drivers & Field Bus Tests
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_bme280.c")
    add_firmware_test(bme280
        SOURCES unit/test_bme280.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_opt3001.c")
    add_firmware_test(opt3001
        SOURCES unit/test_opt3001.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_rain_gauge.c")
    add_firmware_test(rain_gauge
        SOURCES unit/test_rain_gauge.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_modbus_rtu.c")
    add_firmware_test(modbus_rtu
        SOURCES unit/test_modbus_rtu.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_sdi12.c")
    add_firmware_test(sdi12
        SOURCES unit/test_sdi12.c
        LIBRARIES firmware_drivers firmware_core
    )
endif()

# -----------------------------------------------------------------------------
# 9. Telemetry Protocol, LoRaWAN & Flash Storage Tests
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_telemetry_codec.c")
    add_firmware_test(telemetry_codec
        SOURCES unit/test_telemetry_codec.c
        LIBRARIES firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_flash_storage.c")
    add_firmware_test(flash_storage
        SOURCES unit/test_flash_storage.c
        LIBRARIES firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_lorawan_service.c")
    add_firmware_test(lorawan_service
        SOURCES unit/test_lorawan_service.c
        LIBRARIES firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_lorawan_regional.c")
    add_firmware_test(lorawan_regional
        SOURCES unit/test_lorawan_regional.c
        LIBRARIES firmware_middleware firmware_core
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_lorawan_tx_queue.c")
    add_firmware_test(lorawan_tx_queue
        SOURCES unit/test_lorawan_tx_queue.c
        LIBRARIES firmware_middleware firmware_core
    )
endif()

# -----------------------------------------------------------------------------
# 10. JavaScript Gateway Payload Decoder Tests
# -----------------------------------------------------------------------------
find_program(NODE_EXECUTABLE NAMES node nodejs)
if(NODE_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/tools/decoders/test_decoders.js")
    add_test(NAME js_decoders
        COMMAND ${NODE_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/decoders/test_decoders.js
    )
    set_tests_properties(js_decoders PROPERTIES
        TIMEOUT 15
    )
endif()

# -----------------------------------------------------------------------------
# 11. System State Machine Integration Tests (S6-T4.1)
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_state_machine.c")
    add_executable(test_state_machine
        integration/test_state_machine.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/app_state_machine.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/app_fault_handler.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/measurement_scheduler.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/alert_manager.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/rain_algo.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/zambretti.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/trend_detector.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/dew_point.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/moving_avg_filter.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/telemetry_codec.c
        ${UNITY_DIR}/unity.c
    )

    target_include_directories(test_state_machine PRIVATE
        ${UNITY_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_compile_definitions(test_state_machine PRIVATE
        HOST_TEST=1
        UNITY_INCLUDE_CONFIG_H=1
    )

    target_link_libraries(test_state_machine PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
    )

    add_test(NAME StateMachineIntegrationTest COMMAND test_state_machine)
    set_tests_properties(StateMachineIntegrationTest PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endif()

# -----------------------------------------------------------------------------
# 12. System Fault Injection & Resilience Integration Tests (S6-T4.2)
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_fault_injection.c")
    add_executable(test_fault_injection
        integration/test_fault_injection.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/app_state_machine.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/app_fault_handler.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/measurement_scheduler.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/alert_manager.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/rain_algo.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/zambretti.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/trend_detector.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/dew_point.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/moving_avg_filter.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/telemetry_codec.c
        ${UNITY_DIR}/unity.c
    )

    target_include_directories(test_fault_injection PRIVATE
        ${UNITY_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_compile_definitions(test_fault_injection PRIVATE
        HOST_TEST=1
        UNITY_INCLUDE_CONFIG_H=1
    )

    target_link_libraries(test_fault_injection PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
    )

    add_test(NAME FaultInjectionTest COMMAND test_fault_injection)
    set_tests_properties(FaultInjectionTest PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endif()

# -----------------------------------------------------------------------------
# 13. 30-Day Multi-Scenario Climate Simulation Validation (S7-T1.1)
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_simulation_validation.c")
    add_executable(test_simulation_validation
        integration/test_simulation_validation.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/rain_algo.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/zambretti.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/trend_detector.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/dew_point.c
        ${UNITY_DIR}/unity.c
    )

    target_include_directories(test_simulation_validation PRIVATE
        ${UNITY_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_compile_definitions(test_simulation_validation PRIVATE
        HOST_TEST=1
        UNITY_INCLUDE_CONFIG_H=1
    )

    target_link_libraries(test_simulation_validation PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
    )

    add_test(NAME SimulationValidationTest COMMAND test_simulation_validation)
    set_tests_properties(SimulationValidationTest PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endif()

# -----------------------------------------------------------------------------
# 14. Meteorological Contingency Metrics & Skill Verification (S7-T1.2)
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_meteorological_metrics.c")
    add_executable(test_meteorological_metrics
        integration/test_meteorological_metrics.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/rain_algo.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/zambretti.c
        ${CMAKE_SOURCE_DIR}/firmware/app/src/trend_detector.c
        ${CMAKE_SOURCE_DIR}/firmware/middleware/src/dew_point.c
        ${UNITY_DIR}/unity.c
    )

    target_include_directories(test_meteorological_metrics PRIVATE
        ${UNITY_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_compile_definitions(test_meteorological_metrics PRIVATE
        HOST_TEST=1
        UNITY_INCLUDE_CONFIG_H=1
    )

    target_link_libraries(test_meteorological_metrics PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
    )

    add_test(NAME MeteorologicalMetricsTest COMMAND test_meteorological_metrics)
    set_tests_properties(MeteorologicalMetricsTest PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endif()

# -----------------------------------------------------------------------------
# 15. Stop 2 Deep Sleep & Active Cycle Power Profiling (S7-T2.1)
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_power_profiling.c")
    add_executable(test_power_profiling
        integration/test_power_profiling.c
        ${UNITY_DIR}/unity.c
    )

    target_include_directories(test_power_profiling PRIVATE
        ${UNITY_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_compile_definitions(test_power_profiling PRIVATE
        HOST_TEST=1
        UNITY_INCLUDE_CONFIG_H=1
    )

    target_link_libraries(test_power_profiling PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
    )

    add_test(NAME PowerProfilingTest COMMAND test_power_profiling)
    set_tests_properties(PowerProfilingTest PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endif()

# -----------------------------------------------------------------------------
# 16. 24-Hour Total Daily Energy Budget Verification (S7-T2.2)
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_daily_energy_budget.c")
    add_executable(test_daily_energy_budget
        integration/test_daily_energy_budget.c
        ${UNITY_DIR}/unity.c
    )

    target_include_directories(test_daily_energy_budget PRIVATE
        ${UNITY_DIR}
        ${CMAKE_SOURCE_DIR}/firmware/core/inc
        ${CMAKE_SOURCE_DIR}/firmware/drivers/inc
        ${CMAKE_SOURCE_DIR}/firmware/middleware/inc
        ${CMAKE_SOURCE_DIR}/firmware/app/inc
    )

    target_compile_definitions(test_daily_energy_budget PRIVATE
        HOST_TEST=1
        UNITY_INCLUDE_CONFIG_H=1
    )

    target_link_libraries(test_daily_energy_budget PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
    )

    add_test(NAME DailyEnergyBudgetTest COMMAND test_daily_energy_budget)
    set_tests_properties(DailyEnergyBudgetTest PROPERTIES
        TIMEOUT 10
        FAIL_REGULAR_EXPRESSION "FAIL"
    )
endif()




