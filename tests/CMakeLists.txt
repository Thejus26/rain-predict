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


