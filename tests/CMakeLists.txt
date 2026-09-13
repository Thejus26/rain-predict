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





