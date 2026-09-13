/**
 * @file    uart_bus.c
 * @brief   Implementation of Bounded Non-Blocking Dual-Port UART & SDI-12 Bus Driver.
 * @details Target driver for STM32WLE5 USART1 (RS-485 Modbus RTU) and LPUART1 (SDI-12)
 *          with static circular RX ring buffers and complete host simulation backend.
 */

#include "uart_bus.h"
#include <string.h>

#if defined(HAVE_STM32WLXX_HAL)

/* ============================================================================
 * STM32WLE5 Hardware HAL Implementation
 * ============================================================================ */

typedef struct {
    UART_HandleTypeDef  huart;
    uint8_t             rx_storage[UART_BUS_RX_RING_BUFFER_SIZE];
    volatile uint16_t   rx_head;
    volatile uint16_t   rx_tail;
    volatile uint16_t   rx_count;
    uint8_t             rx_byte_buf;
    bool                is_initialized;
    uint32_t            baud_rate;
    uint8_t             data_bits;
    uint8_t             parity;
    uint8_t             stop_bits;
} uart_port_state_t;

static uart_port_state_t s_ports[UART_PORT_MAX];

/**
 * @brief Software microsecond delay for RS-485 transceiver guard timing.
 * @param[in] us Duration in microseconds.
 */
static void uart_bus_delay_us(uint32_t us) {
    /* 48 MHz CPU clock: ~12 cycles per iteration */
    uint32_t cycles = us * 12U;
    while (cycles > 0U) {
        __NOP();
        cycles--;
    }
}

status_t uart_bus_init(uart_port_t port,
                       uint32_t baud_rate,
                       uint8_t data_bits,
                       uint8_t parity,
                       uint8_t stop_bits) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (baud_rate == 0U || (data_bits != 7U && data_bits != 8U) ||
        parity > (uint8_t)UART_PARITY_ODD || (stop_bits != 1U && stop_bits != 2U)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    uart_port_state_t *p_state = &s_ports[port];

    /* 1. Clear circular ring buffer state */
    p_state->rx_head   = 0U;
    p_state->rx_tail   = 0U;
    p_state->rx_count  = 0U;
    p_state->baud_rate = baud_rate;
    p_state->data_bits = data_bits;
    p_state->parity    = parity;
    p_state->stop_bits = stop_bits;

    GPIO_InitTypeDef gpio_init = {0};

    if (port == UART_PORT_RS485) {
        /* Enable GPIOA and USART1 peripheral clocks */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();

        /* Configure Direction Control Pin PA1 (Push-Pull, default LOW / RX) */
        HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_RESET);
        gpio_init.Pin   = PIN_RS485_DIR_PIN;
        gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull  = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(PIN_RS485_DIR_PORT, &gpio_init);

        /* Configure USART1 TX (PA2) and RX (PA3) alternate function pins */
        gpio_init.Pin       = PIN_RS485_TX_PIN | PIN_RS485_RX_PIN;
        gpio_init.Mode      = GPIO_MODE_AF_PP;
        gpio_init.Pull      = GPIO_PULLUP;
        gpio_init.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = PIN_RS485_TX_AF;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        p_state->huart.Instance = USART1;
    } else {
        /* Enable GPIOC and LPUART1 peripheral clocks */
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_LPUART1_CLK_ENABLE();

        /* Configure Direction Control Pin PC2 (Push-Pull, default LOW / RX) */
        HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_RESET);
        gpio_init.Pin   = PIN_SDI12_DIR_PIN;
        gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull  = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(PIN_SDI12_DIR_PORT, &gpio_init);

        /* Configure LPUART1 TX (PC0) and RX (PC1) alternate function pins */
        gpio_init.Pin       = PIN_SDI12_TX_PIN | PIN_SDI12_RX_PIN;
        gpio_init.Mode      = GPIO_MODE_AF_PP;
        gpio_init.Pull      = GPIO_PULLUP;
        gpio_init.Speed     = GPIO_SPEED_FREQ_HIGH;
        gpio_init.Alternate = PIN_SDI12_TX_AF;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        p_state->huart.Instance = LPUART1;
    }

    /* 2. Configure UART Peripheral Parameters */
    p_state->huart.Init.BaudRate = baud_rate;

    if (parity == (uint8_t)UART_PARITY_NONE) {
        p_state->huart.Init.WordLength = (data_bits == 7U) ? UART_WORDLENGTH_7B : UART_WORDLENGTH_8B;
        p_state->huart.Init.Parity     = UART_PARITY_NONE;
    } else {
        /* Parity bit is included in STM32 WordLength frame count */
        p_state->huart.Init.WordLength = (data_bits == 7U) ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
        p_state->huart.Init.Parity     = (parity == (uint8_t)UART_PARITY_EVEN) ? UART_PARITY_EVEN : UART_PARITY_ODD;
    }

    p_state->huart.Init.StopBits          = (stop_bits == 2U) ? UART_STOPBITS_2 : UART_STOPBITS_1;
    p_state->huart.Init.Mode              = UART_MODE_TX_RX;
    p_state->huart.Init.HwFlowCtl         = UART_HWCONTROL_NONE;
    p_state->huart.Init.OverSampling      = UART_OVERSAMPLING_16;
    p_state->huart.Init.OneBitSampling    = UART_ONE_BIT_SAMPLE_DISABLE;
    p_state->huart.Init.ClockPrescaler    = UART_PRESCALER_DIV1;

    if (HAL_UART_Init(&p_state->huart) != HAL_OK) {
        return STATUS_ERR_UART_BUS;
    }

    /* 3. Enable Peripheral NVIC Interrupts */
    IRQn_Type irq = (port == UART_PORT_RS485) ? USART1_IRQn : LPUART1_IRQn;
    HAL_NVIC_SetPriority(irq, 1, 0);
    HAL_NVIC_EnableIRQ(irq);

    /* 4. Arm Non-blocking 1-byte Reception */
    (void)HAL_UART_Receive_IT(&p_state->huart, &p_state->rx_byte_buf, 1U);

    p_state->is_initialized = true;
    return STATUS_OK;
}

status_t uart_bus_deinit(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    uart_port_state_t *p_state = &s_ports[port];

    if (p_state->is_initialized) {
        IRQn_Type irq = (port == UART_PORT_RS485) ? USART1_IRQn : LPUART1_IRQn;
        HAL_NVIC_DisableIRQ(irq);

        (void)HAL_UART_DeInit(&p_state->huart);

        if (port == UART_PORT_RS485) {
            __HAL_RCC_USART1_CLK_DISABLE();
        } else {
            __HAL_RCC_LPUART1_CLK_DISABLE();
        }
        p_state->is_initialized = false;
    }
    return STATUS_OK;
}

status_t uart_bus_set_direction(uart_port_t port, uart_dir_t dir) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (port == UART_PORT_RS485) {
        HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN,
                          (dir == UART_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN,
                          (dir == UART_DIR_TX) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    return STATUS_OK;
}

status_t uart_bus_transmit(uart_port_t port,
                           const uint8_t *p_data,
                           uint16_t length,
                           uint32_t timeout_ms) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL || length == 0U) {
        return STATUS_ERR_NULL_PTR;
    }
    uart_port_state_t *p_state = &s_ports[port];
    if (!p_state->is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    /* 1. Pre-transmission direction gating */
    if (port == UART_PORT_RS485) {
        HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_SET);
        uart_bus_delay_us(UART_BUS_RS485_PRE_DELAY_US);
    } else {
        HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_SET);
    }

    /* 2. Transmit frame with bounded timeout */
    HAL_StatusTypeDef hal_status = HAL_UART_Transmit(&p_state->huart,
                                                     (uint8_t *)p_data,
                                                     length,
                                                     timeout_ms);

    /* 3. Post-transmission direction gating */
    if (port == UART_PORT_RS485) {
        uart_bus_delay_us(UART_BUS_RS485_POST_DELAY_US);
        HAL_GPIO_WritePin(PIN_RS485_DIR_PORT, PIN_RS485_DIR_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_RESET);
    }

    if (hal_status == HAL_OK) {
        return STATUS_OK;
    } else if (hal_status == HAL_TIMEOUT) {
        return STATUS_ERR_TIMEOUT;
    } else {
        return STATUS_ERR_UART_BUS;
    }
}

status_t uart_bus_receive(uart_port_t port,
                          uint8_t *p_data,
                          uint16_t length,
                          uint16_t *p_bytes_received,
                          uint32_t timeout_ms) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL || p_bytes_received == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    uart_port_state_t *p_state = &s_ports[port];
    *p_bytes_received = 0U;

    if (!p_state->is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (length == 0U) {
        return STATUS_OK;
    }

    /* Bounded non-blocking wait for requested bytes */
    uint32_t start_tick = HAL_GetTick();
    while (p_state->rx_count < length) {
        if ((HAL_GetTick() - start_tick) >= timeout_ms) {
            break;
        }
    }

    /* Pop available bytes from static circular ring buffer */
    uint16_t copy_count = (p_state->rx_count < length) ? p_state->rx_count : length;
    for (uint16_t i = 0; i < copy_count; i++) {
        p_data[i] = p_state->rx_storage[p_state->rx_tail];
        p_state->rx_tail = (uint16_t)((p_state->rx_tail + 1U) % UART_BUS_RX_RING_BUFFER_SIZE);

        __disable_irq();
        p_state->rx_count--;
        __enable_irq();
    }

    *p_bytes_received = copy_count;
    return (copy_count > 0U) ? STATUS_OK : STATUS_ERR_TIMEOUT;
}

status_t uart_bus_sdi12_send_break(void) {
    /* 1. Assert direction pin PC2 */
    HAL_GPIO_WritePin(PIN_SDI12_DIR_PORT, PIN_SDI12_DIR_PIN, GPIO_PIN_SET);

    /* 2. Configure PC0 as Output Push-Pull and drive HIGH (Break Spacing) for 13 ms */
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin   = PIN_SDI12_TX_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    HAL_GPIO_WritePin(GPIOC, PIN_SDI12_TX_PIN, GPIO_PIN_SET);
    HAL_Delay(UART_BUS_SDI12_BREAK_MS);

    /* 3. Drive PC0 LOW (Marking) for 9 ms */
    HAL_GPIO_WritePin(GPIOC, PIN_SDI12_TX_PIN, GPIO_PIN_RESET);
    HAL_Delay(UART_BUS_SDI12_MARK_MS);

    /* 4. Restore PC0 to Alternate Function (LPUART1 TX AF8) */
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Alternate = PIN_SDI12_TX_AF;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    return STATUS_OK;
}

status_t uart_bus_flush(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    uart_port_state_t *p_state = &s_ports[port];

    __disable_irq();
    p_state->rx_head  = 0U;
    p_state->rx_tail  = 0U;
    p_state->rx_count = 0U;
    __enable_irq();

    if (p_state->is_initialized) {
        __HAL_UART_CLEAR_OREFLAG(&p_state->huart);
    }
    return STATUS_OK;
}

uint16_t uart_bus_get_available(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_ports[port].rx_count;
}

void uart_bus_rx_isr_handler(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return;
    }
    uart_port_state_t *p_state = &s_ports[port];

    /* Store byte in circular ring buffer */
    if (p_state->rx_count < UART_BUS_RX_RING_BUFFER_SIZE) {
        p_state->rx_storage[p_state->rx_head] = p_state->rx_byte_buf;
        p_state->rx_head = (uint16_t)((p_state->rx_head + 1U) % UART_BUS_RX_RING_BUFFER_SIZE);
        p_state->rx_count++;
    }

    /* Re-arm interrupt reception for next incoming byte */
    (void)HAL_UART_Receive_IT(&p_state->huart, &p_state->rx_byte_buf, 1U);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart != NULL) {
        if (huart->Instance == USART1) {
            uart_bus_rx_isr_handler(UART_PORT_RS485);
        } else if (huart->Instance == LPUART1) {
            uart_bus_rx_isr_handler(UART_PORT_SDI12);
        }
    }
}

#else

/* ============================================================================
 * Host Simulation & Unit Testing Backend
 * ============================================================================ */

#define SIM_UART_TX_BUFFER_SIZE     512U

typedef struct {
    uint8_t             rx_storage[UART_BUS_RX_RING_BUFFER_SIZE];
    uint16_t            rx_head;
    uint16_t            rx_tail;
    uint16_t            rx_count;
    uint8_t             rx_byte_buf;

    uint8_t             tx_buffer[SIM_UART_TX_BUFFER_SIZE];
    uint16_t            tx_count;

    uart_dir_t          direction;
    uint32_t            active_fault;
    bool                is_initialized;

    uint32_t            baud_rate;
    uint8_t             data_bits;
    uint8_t             parity;
    uint8_t             stop_bits;
} sim_uart_port_t;

static sim_uart_port_t s_sim_ports[UART_PORT_MAX];
static bool s_sim_break_sent = false;
static uint32_t s_sim_break_duration_ms = 0;
static uint32_t s_sim_mark_duration_ms = 0;
static uint32_t s_sim_pre_delay_us = 0;
static uint32_t s_sim_post_delay_us = 0;
static bool s_sim_auto_direction = true;

void uart_bus_test_reset(void) {
    (void)memset(s_sim_ports, 0, sizeof(s_sim_ports));
    for (size_t i = 0; i < (size_t)UART_PORT_MAX; i++) {
        s_sim_ports[i].direction      = UART_DIR_RX;
        s_sim_ports[i].active_fault   = 0U;
        s_sim_ports[i].is_initialized = true;
        s_sim_ports[i].baud_rate      = (i == (size_t)UART_PORT_RS485) ? 9600U : 1200U;
        s_sim_ports[i].data_bits      = (i == (size_t)UART_PORT_RS485) ? 8U : 7U;
        s_sim_ports[i].parity         = (i == (size_t)UART_PORT_RS485) ? (uint8_t)UART_PARITY_NONE : (uint8_t)UART_PARITY_EVEN;
        s_sim_ports[i].stop_bits      = 1U;
    }
    s_sim_break_sent        = false;
    s_sim_break_duration_ms = 0U;
    s_sim_mark_duration_ms  = 0U;
    s_sim_pre_delay_us      = 0U;
    s_sim_post_delay_us     = 0U;
    s_sim_auto_direction    = true;
}

status_t uart_bus_test_inject_rx(uart_port_t port, const uint8_t *p_data, uint16_t length) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    for (uint16_t i = 0; i < length; i++) {
        status_t status = uart_bus_test_inject_rx_byte(port, p_data[i]);
        if (status != STATUS_OK) {
            return status;
        }
    }
    return STATUS_OK;
}

status_t uart_bus_test_inject_rx_byte(uart_port_t port, uint8_t byte) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    sim_uart_port_t *p_port = &s_sim_ports[port];

    if (p_port->rx_count >= UART_BUS_RX_RING_BUFFER_SIZE) {
        return STATUS_ERR_BUSY;
    }

    p_port->rx_storage[p_port->rx_head] = byte;
    p_port->rx_head = (uint16_t)((p_port->rx_head + 1U) % UART_BUS_RX_RING_BUFFER_SIZE);
    p_port->rx_count++;
    return STATUS_OK;
}

uint16_t uart_bus_test_get_tx_bytes(uart_port_t port, uint8_t *p_out, uint16_t max_len) {
    if (port >= UART_PORT_MAX || p_out == NULL) {
        return 0U;
    }
    sim_uart_port_t *p_port = &s_sim_ports[port];
    uint16_t copy_len = (p_port->tx_count < max_len) ? p_port->tx_count : max_len;
    (void)memcpy(p_out, p_port->tx_buffer, copy_len);
    return copy_len;
}

uint16_t uart_bus_test_get_tx_count(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].tx_count;
}

uint16_t uart_bus_test_get_rx_count(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].rx_count;
}

uart_dir_t uart_bus_test_get_direction(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return UART_DIR_RX;
    }
    return s_sim_ports[port].direction;
}

void uart_bus_test_set_direction(uart_port_t port, uart_dir_t dir) {
    if (port < UART_PORT_MAX) {
        s_sim_ports[port].direction = dir;
    }
}

void uart_bus_test_inject_fault(uart_port_t port, uint32_t fault) {
    if (port < UART_PORT_MAX) {
        s_sim_ports[port].active_fault = fault;
    }
}

void uart_bus_test_clear_faults(uart_port_t port) {
    if (port < UART_PORT_MAX) {
        s_sim_ports[port].active_fault = 0U;
    }
}

bool uart_bus_test_is_initialized(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return false;
    }
    return s_sim_ports[port].is_initialized;
}

uint32_t uart_bus_test_get_baud_rate(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].baud_rate;
}

uint8_t uart_bus_test_get_data_bits(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].data_bits;
}

uint8_t uart_bus_test_get_parity(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].parity;
}

uint8_t uart_bus_test_get_stop_bits(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].stop_bits;
}

bool uart_bus_test_get_break_sent(void) {
    return s_sim_break_sent;
}

uint32_t uart_bus_test_get_break_duration_ms(void) {
    return s_sim_break_duration_ms;
}

uint32_t uart_bus_test_get_mark_duration_ms(void) {
    return s_sim_mark_duration_ms;
}

uint32_t uart_bus_test_get_pre_delay_us(void) {
    return s_sim_pre_delay_us;
}

uint32_t uart_bus_test_get_post_delay_us(void) {
    return s_sim_post_delay_us;
}

void uart_bus_test_set_auto_direction(bool enable) {
    s_sim_auto_direction = enable;
}

bool uart_bus_test_get_auto_direction(void) {
    return s_sim_auto_direction;
}

status_t uart_bus_init(uart_port_t port,
                       uint32_t baud_rate,
                       uint8_t data_bits,
                       uint8_t parity,
                       uint8_t stop_bits) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (baud_rate == 0U || (data_bits != 7U && data_bits != 8U) ||
        parity > (uint8_t)UART_PARITY_ODD || (stop_bits != 1U && stop_bits != 2U)) {
        return STATUS_ERR_INVALID_PARAM;
    }

    sim_uart_port_t *p_port = &s_sim_ports[port];
    p_port->rx_head        = 0U;
    p_port->rx_tail        = 0U;
    p_port->rx_count       = 0U;
    p_port->tx_count       = 0U;
    p_port->baud_rate      = baud_rate;
    p_port->data_bits      = data_bits;
    p_port->parity         = parity;
    p_port->stop_bits      = stop_bits;
    p_port->is_initialized = true;

    return STATUS_OK;
}

status_t uart_bus_deinit(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_sim_ports[port].is_initialized = false;
    return STATUS_OK;
}

status_t uart_bus_set_direction(uart_port_t port, uart_dir_t dir) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    s_sim_ports[port].direction = dir;
    return STATUS_OK;
}

status_t uart_bus_transmit(uart_port_t port,
                           const uint8_t *p_data,
                           uint16_t length,
                           uint32_t timeout_ms) {
    (void)timeout_ms;
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL || length == 0U) {
        return STATUS_ERR_NULL_PTR;
    }

    sim_uart_port_t *p_port = &s_sim_ports[port];
    if (!p_port->is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }

    if (p_port->active_fault == 1U) { /* MOCK_UART_FAULT_TIMEOUT */
        return STATUS_ERR_TIMEOUT;
    }
    if (p_port->active_fault == 5U) { /* MOCK_UART_FAULT_TX_COLLISION */
        return STATUS_ERR_UART_BUS;
    }

    if (s_sim_auto_direction) {
        p_port->direction  = UART_DIR_TX;
        s_sim_pre_delay_us = UART_BUS_RS485_PRE_DELAY_US;
    } else {
        if (port == UART_PORT_RS485 && p_port->direction != UART_DIR_TX) {
            return STATUS_ERR_UART_BUS;
        }
    }

    uint16_t available = (uint16_t)(SIM_UART_TX_BUFFER_SIZE - p_port->tx_count);
    uint16_t write_len = (length < available) ? length : available;

    (void)memcpy(&p_port->tx_buffer[p_port->tx_count], p_data, write_len);
    p_port->tx_count = (uint16_t)(p_port->tx_count + write_len);

    if (s_sim_auto_direction) {
        s_sim_post_delay_us = UART_BUS_RS485_POST_DELAY_US;
        p_port->direction   = UART_DIR_RX;
    }

    return (write_len == length) ? STATUS_OK : STATUS_ERR_BUSY;
}

status_t uart_bus_receive(uart_port_t port,
                          uint8_t *p_data,
                          uint16_t length,
                          uint16_t *p_bytes_received,
                          uint32_t timeout_ms) {
    (void)timeout_ms;
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (p_data == NULL || p_bytes_received == NULL) {
        return STATUS_ERR_NULL_PTR;
    }
    *p_bytes_received = 0U;

    sim_uart_port_t *p_port = &s_sim_ports[port];
    if (!p_port->is_initialized) {
        return STATUS_ERR_INVALID_PARAM;
    }
    if (length == 0U) {
        return STATUS_OK;
    }

    if (p_port->active_fault == 1U || p_port->rx_count == 0U) { /* TIMEOUT */
        return STATUS_ERR_TIMEOUT;
    }
    if (p_port->active_fault == 2U || p_port->active_fault == 3U) { /* FRAMING / PARITY */
        return STATUS_ERR_UART_BUS;
    }
    if (p_port->active_fault == 4U) { /* BUFFER_OVERFLOW */
        return STATUS_ERR_OVERFLOW;
    }

    uint16_t bytes_to_read = (length < p_port->rx_count) ? length : p_port->rx_count;
    for (uint16_t i = 0; i < bytes_to_read; i++) {
        p_data[i] = p_port->rx_storage[p_port->rx_tail];
        p_port->rx_tail = (uint16_t)((p_port->rx_tail + 1U) % UART_BUS_RX_RING_BUFFER_SIZE);
        p_port->rx_count--;
    }

    *p_bytes_received = bytes_to_read;
    return STATUS_OK;
}

status_t uart_bus_sdi12_send_break(void) {
    s_sim_break_sent        = true;
    s_sim_break_duration_ms = UART_BUS_SDI12_BREAK_MS;
    s_sim_mark_duration_ms  = UART_BUS_SDI12_MARK_MS;
    s_sim_ports[UART_PORT_SDI12].direction = UART_DIR_TX;
    return STATUS_OK;
}

status_t uart_bus_flush(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return STATUS_ERR_INVALID_PARAM;
    }
    sim_uart_port_t *p_port = &s_sim_ports[port];
    p_port->rx_head  = 0U;
    p_port->rx_tail  = 0U;
    p_port->rx_count = 0U;
    p_port->tx_count = 0U;
    return STATUS_OK;
}

uint16_t uart_bus_get_available(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return 0U;
    }
    return s_sim_ports[port].rx_count;
}

void uart_bus_rx_isr_handler(uart_port_t port) {
    if (port >= UART_PORT_MAX) {
        return;
    }
    sim_uart_port_t *p_port = &s_sim_ports[port];
    if (p_port->rx_count < UART_BUS_RX_RING_BUFFER_SIZE) {
        p_port->rx_storage[p_port->rx_head] = p_port->rx_byte_buf;
        p_port->rx_head = (uint16_t)((p_port->rx_head + 1U) % UART_BUS_RX_RING_BUFFER_SIZE);
        p_port->rx_count++;
    }
}

#endif /* HAVE_STM32WLXX_HAL */
