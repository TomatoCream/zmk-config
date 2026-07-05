/*
 * PS/2 transport over a UART peripheral (nRF52).
 *
 * Reading: the UART RX pin is mapped to the PS/2 data line and the UART is
 * configured for even parity. PS/2 uses odd parity, so every valid byte
 * raises a parity error, which we treat as a validity check.
 *
 * Writing: the UART pinctrl is switched to its sleep state to release the
 * data pin, then the clock-inhibit sequence and data bits are bit-banged
 * through GPIO, clocked by the device via a falling-edge interrupt on SCL.
 *
 * Adapted from infused-kim/kb_zmk_ps2_mouse_trackpoint_driver (ps2_uart.c),
 * reduced to the standard Zephyr ps2_driver_api.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_trackpoint_ps2_uart

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/ps2.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <hal/nrf_uarte.h>

LOG_MODULE_REGISTER(ps2_uart, CONFIG_ZMK_TRACKPOINT_LOG_LEVEL);

PINCTRL_DT_DEFINE(DT_INST_PARENT(0));

#define PS2_UART_WRITE_MAX_RETRY 5

#define PS2_UART_DATA_QUEUE_SIZE 100

// Queue for background PS/2 processing work at low priority so that busy
// periods finish before we start timing-sensitive writes.
#define PS2_UART_WORK_QUEUE_PRIORITY 10
#define PS2_UART_WORK_QUEUE_STACK_SIZE 1024

// Queue for calling the zephyr ps2 callback outside of ISR context, but at
// a high priority since PS/2 packets must be handled quickly.
#define PS2_UART_WORK_QUEUE_CB_PRIORITY 2
#define PS2_UART_WORK_QUEUE_CB_STACK_SIZE 1024

/*
 * PS/2 frame positions
 */

#define PS2_UART_POS_START 0
#define PS2_UART_POS_DATA_FIRST 1
#define PS2_UART_POS_DATA_LAST 8
#define PS2_UART_POS_PARITY 9
#define PS2_UART_POS_STOP 10
#define PS2_UART_POS_ACK 11 // Write mode only

#define PS2_UART_RESP_ACK 0xfa
#define PS2_UART_RESP_RESEND 0xfe
#define PS2_UART_RESP_FAILURE 0xfc

/*
 * PS/2 timings
 */

#define PS2_UART_TIMING_SCL_CYCLE_LEN 69

// The minimum clock-inhibit time to start a write is 100us, but real
// trackpoints respond most reliably to ~500us.
#define PS2_UART_TIMING_SCL_INHIBITION_MIN 100
#define PS2_UART_TIMING_SCL_INHIBITION (5 * PS2_UART_TIMING_SCL_INHIBITION_MIN)

// PS/2 clocks at 10-16.7 kHz, so edges arrive within 60-100us.
#define PS2_UART_TIMING_SCL_CYCLE_MAX 100

// After releasing an inhibited clock, devices can take several ms to start
// clocking, especially when interrupting an ongoing read.
#define PS2_UART_TIMING_SCL_INHIBITION_RESP_MAX 3000
#define PS2_UART_TIMEOUT_WRITE_SCL_START K_USEC(PS2_UART_TIMING_SCL_INHIBITION_RESP_MAX)

// Max time between clock edges during a write, with interrupt-latency slack.
#define PS2_UART_TIMEOUT_WRITE_SCL K_USEC(PS2_UART_TIMING_SCL_CYCLE_MAX + 50)

// Inhibition + response + 11 bits + 2 cycles of slack
#define PS2_UART_TIMING_WRITE_MAX_TIME                                                             \
    (PS2_UART_TIMING_SCL_INHIBITION + PS2_UART_TIMING_SCL_INHIBITION_RESP_MAX +                    \
     11 * PS2_UART_TIMING_SCL_CYCLE_MAX + 2 * PS2_UART_TIMING_SCL_CYCLE_MAX)

// Devices should respond to a write within 20ms per spec, but real
// trackpoints can take much longer.
#define PS2_UART_TIMEOUT_WRITE_AWAIT_RESPONSE K_MSEC(300)

// How long a blocking ps2_read waits for data.
#define PS2_UART_TIMEOUT_READ K_SECONDS(2)

#define PS2_UART_TIMEOUT_WRITE_BLOCKING K_USEC(PS2_UART_TIMING_WRITE_MAX_TIME)

typedef enum {
    PS2_UART_WRITE_STATUS_INACTIVE,
    PS2_UART_WRITE_STATUS_ACTIVE,
    PS2_UART_WRITE_STATUS_SUCCESS,
    PS2_UART_WRITE_STATUS_FAILURE,
} ps2_uart_write_status;

struct ps2_uart_data_queue_item {
    uint8_t byte;
};

struct ps2_uart_config {
    const struct device *uart_dev;
    struct gpio_dt_spec scl_gpio;
    struct gpio_dt_spec sda_gpio;
    const struct pinctrl_dev_config *pcfg;
};

struct ps2_uart_data {
    const struct device *dev;

    struct gpio_callback scl_cb_data;

    struct k_work callback_work;
    uint8_t callback_byte;
    ps2_callback_t callback_isr;
    bool callback_enabled;

    struct k_msgq data_queue;
    char data_queue_buffer[PS2_UART_DATA_QUEUE_SIZE * sizeof(struct ps2_uart_data_queue_item)];

    ps2_uart_write_status cur_write_status;
    uint8_t cur_write_byte;
    int cur_write_pos;
    bool write_awaits_resp;
    uint8_t write_awaits_resp_byte;
    struct k_sem write_awaits_resp_sem;
    struct k_sem write_lock;
    struct k_work_delayable write_scl_timeout;
};

static const struct ps2_uart_config ps2_uart_config = {
    .uart_dev = DEVICE_DT_GET(DT_INST_PARENT(0)),
    .scl_gpio = GPIO_DT_SPEC_INST_GET(0, scl_gpios),
    .sda_gpio = GPIO_DT_SPEC_INST_GET(0, sda_gpios),
    .pcfg = PINCTRL_DT_DEV_CONFIG_GET(DT_INST_PARENT(0)),
};

static struct ps2_uart_data ps2_uart_data = {
    .callback_byte = 0x0,
    .callback_isr = NULL,
    .callback_enabled = false,

    .cur_write_status = PS2_UART_WRITE_STATUS_INACTIVE,
    .cur_write_byte = 0x0,
    .cur_write_pos = 0,
    .write_awaits_resp = false,
    .write_awaits_resp_byte = 0x0,
};

K_THREAD_STACK_DEFINE(ps2_uart_work_queue_stack_area, PS2_UART_WORK_QUEUE_STACK_SIZE);
static struct k_work_q ps2_uart_work_queue;

K_THREAD_STACK_DEFINE(ps2_uart_work_queue_cb_stack_area, PS2_UART_WORK_QUEUE_CB_STACK_SIZE);
static struct k_work_q ps2_uart_work_queue_cb;

static int ps2_uart_write_byte(uint8_t byte);

/*
 * Helpers
 */

#define PS2_UART_GET_BIT(data, bit_pos) (((data) >> (bit_pos)) & 0x1)

static int ps2_uart_get_sda(void) {
    const struct ps2_uart_config *config = &ps2_uart_config;

    return gpio_pin_get_dt(&config->sda_gpio);
}

static void ps2_uart_set_scl(int state) {
    const struct ps2_uart_config *config = &ps2_uart_config;

    gpio_pin_set_dt(&config->scl_gpio, state);
}

static void ps2_uart_set_sda(int state) {
    const struct ps2_uart_config *config = &ps2_uart_config;

    gpio_pin_set_dt(&config->sda_gpio, state);
}

static int ps2_uart_configure_pin_scl(gpio_flags_t flags, char *descr) {
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    err = gpio_pin_configure_dt(&config->scl_gpio, flags);
    if (err) {
        LOG_ERR("failed to configure SCL GPIO pin to %s (err %d)", descr, err);
    }

    return err;
}

static int ps2_uart_configure_pin_scl_input(void) {
    return ps2_uart_configure_pin_scl((GPIO_INPUT), "input");
}

static int ps2_uart_configure_pin_scl_output(void) {
    return ps2_uart_configure_pin_scl((GPIO_OUTPUT_HIGH), "output");
}

static int ps2_uart_configure_pin_sda(gpio_flags_t flags, char *descr) {
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    err = gpio_pin_configure_dt(&config->sda_gpio, flags);
    if (err) {
        LOG_ERR("failed to configure SDA GPIO pin to %s (err %d)", descr, err);
    }

    return err;
}

static int ps2_uart_configure_pin_sda_input(void) {
    return ps2_uart_configure_pin_sda((GPIO_INPUT), "input");
}

static int ps2_uart_configure_pin_sda_output(void) {
    return ps2_uart_configure_pin_sda((GPIO_OUTPUT_HIGH), "output");
}

static int ps2_uart_set_scl_callback_enabled(bool enabled) {
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    if (enabled) {
        err = gpio_pin_interrupt_configure_dt(&config->scl_gpio, (GPIO_INT_EDGE_FALLING));
        if (err) {
            LOG_ERR("failed to enable interrupt on SCL GPIO pin (err %d)", err);
            return err;
        }
    } else {
        err = gpio_pin_interrupt_configure_dt(&config->scl_gpio, (GPIO_INT_DISABLE));
        if (err) {
            LOG_ERR("failed to disable interrupt on SCL GPIO pin (err %d)", err);
            return err;
        }
    }

    return err;
}

static int ps2_uart_set_mode_read(void) {
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    // Give the SDA pin back to the UART peripheral
    err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
    if (err < 0) {
        LOG_ERR("Could not switch pinctrl state to DEFAULT: %d", err);
        return err;
    }

    ps2_uart_set_scl_callback_enabled(false);

    uart_irq_rx_enable(config->uart_dev);

    return err;
}

static int ps2_uart_set_mode_write(void) {
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    // Move the UART to unused pins so that we can control the real pins
    // through GPIO
    err = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_SLEEP);
    if (err < 0) {
        LOG_ERR("Could not switch pinctrl state to SLEEP: %d", err);
        return err;
    }

    // Unintuitively, this has to be done AFTER applying the pinctrl state,
    // otherwise GPIO can't drive the data pin
    uart_irq_rx_disable(config->uart_dev);

    ps2_uart_set_scl_callback_enabled(false);
    ps2_uart_configure_pin_scl_output();
    ps2_uart_configure_pin_sda_output();

    return err;
}

static bool ps2_uart_get_byte_parity(uint8_t byte) {
    // gcc parity returns 1 for an odd number of set bits, PS/2 wants the
    // parity bit to make the total odd.
    return !__builtin_parity(byte);
}

static int ps2_uart_data_queue_get_next(uint8_t *dst_byte, k_timeout_t timeout) {
    struct ps2_uart_data *data = &ps2_uart_data;
    struct ps2_uart_data_queue_item queue_data;
    int ret;

    ret = k_msgq_get(&data->data_queue, &queue_data, timeout);
    if (ret != 0) {
        LOG_WRN("Data queue timed out...");
        return -ETIMEDOUT;
    }

    *dst_byte = queue_data.byte;

    return 0;
}

static void ps2_uart_data_queue_empty(void) {
    struct ps2_uart_data *data = &ps2_uart_data;

    k_msgq_purge(&data->data_queue);
}

static void ps2_uart_data_queue_add(uint8_t byte) {
    struct ps2_uart_data *data = &ps2_uart_data;
    int ret;

    struct ps2_uart_data_queue_item queue_data;
    queue_data.byte = byte;

    LOG_DBG("Adding byte to data queue: 0x%x", byte);

    for (int i = 0; i < 2; i++) {
        ret = k_msgq_put(&data->data_queue, &queue_data, K_NO_WAIT);
        if (ret == 0) {
            break;
        }

        LOG_WRN("Data queue full. Removing oldest item.");

        uint8_t tmp_byte;
        ps2_uart_data_queue_get_next(&tmp_byte, K_NO_WAIT);
    }

    if (ret != 0) {
        LOG_ERR("Failed to add byte 0x%x to the data queue.", byte);
    }
}

/*
 * Reading PS/2 data
 */

static const char *ps2_uart_read_get_error_str(int err) {
    switch (err) {
    case UART_ERROR_OVERRUN:
        return "Overrun error";
    case UART_ERROR_PARITY:
        return "Parity error";
    case UART_ERROR_FRAMING:
        return "Framing error";
    case UART_BREAK:
        return "Break interrupt";
    case UART_ERROR_COLLISION:
        return "Collision error";
    default:
        return "Unknown error";
    }
}

static int ps2_uart_read_err_check(const struct device *dev) {
    int err = uart_err_check(dev);

    // The UART is configured for even parity, but PS/2 uses odd parity, so
    // valid bytes always raise a parity error. No parity error means the
    // byte really had even parity and is invalid.
    if ((err & NRF_UARTE_ERROR_PARITY_MASK) == 0) {
        err = UART_ERROR_PARITY;
    } else if (err & NRF_UARTE_ERROR_OVERRUN_MASK) {
        err = UART_ERROR_OVERRUN;
    } else if (err & NRF_UARTE_ERROR_FRAMING_MASK) {
        err = UART_ERROR_FRAMING;
    } else if (err & NRF_UARTE_ERROR_BREAK_MASK) {
        err = UART_BREAK;
    } else { // No errors
        err = 0;
    }

    return err;
}

static void ps2_uart_read_process_received_byte(uint8_t byte) {
    struct ps2_uart_data *data = &ps2_uart_data;
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    LOG_DBG("UART Received: 0x%x", byte);

    err = ps2_uart_read_err_check(config->uart_dev);
    if (err != 0) {
        const char *err_str = ps2_uart_read_get_error_str(err);

        // 0xfa acks frequently arrive with framing errors; not a real error.
        if (!(byte == PS2_UART_RESP_ACK && err == UART_ERROR_FRAMING)) {
            LOG_WRN("UART RX detected error for byte 0x%x: %s (%d)", byte, err_str, err);
        }
    }

    // If write_byte_await_response() is waiting, notify the blocked write
    // of the response.
    if (data->write_awaits_resp) {
        data->write_awaits_resp_byte = byte;
        data->write_awaits_resp = false;
        k_sem_give(&data->write_awaits_resp_sem);

        // Don't forward protocol responses to the callback / read queue.
        if (byte == PS2_UART_RESP_ACK || byte == PS2_UART_RESP_RESEND ||
            byte == PS2_UART_RESP_FAILURE) {
            return;
        }
    }

    if (data->callback_isr != NULL && data->callback_enabled) {
        // Hand off to a worker so the callback can't block the interrupt.
        data->callback_byte = byte;
        k_work_submit_to_queue(&ps2_uart_work_queue_cb, &data->callback_work);
    } else {
        ps2_uart_data_queue_add(byte);
    }
}

static void ps2_uart_read_interrupt_handler(const struct device *uart_dev, void *user_data) {
    uint8_t byte;

    int byte_len = uart_fifo_read(uart_dev, &byte, 1);
    if (byte_len < 1) {
        LOG_ERR("UART read failed with error: %d", byte_len);
        return;
    }

    ps2_uart_read_process_received_byte(byte);
}

static void ps2_uart_interrupt_handler(const struct device *uart_dev, void *user_data) {
    int err;

    err = uart_irq_update(uart_dev);
    if (err != 1) {
        LOG_ERR("uart_irq_update returned: %d", err);
        return;
    }

    while (uart_irq_rx_ready(uart_dev)) {
        ps2_uart_read_interrupt_handler(uart_dev, user_data);
    }
}

static void ps2_uart_read_callback_work_handler(struct k_work *work) {
    struct ps2_uart_data *data = &ps2_uart_data;

    data->callback_isr(data->dev, data->callback_byte);
    data->callback_byte = 0x0;
}

/*
 * Writing PS/2 data
 */

static int ps2_uart_write_byte_await_response(uint8_t byte);
static int ps2_uart_write_byte_blocking(uint8_t byte);
static int ps2_uart_write_byte_start(uint8_t byte);
static void ps2_uart_write_finish(bool successful, char *descr);

// Error writing to the device (no clock, invalid ack, ...)
#define PS2_UART_E_WRITE_TRANSMIT 1
// Semaphore timeout during a blocking write
#define PS2_UART_E_WRITE_SEM_TIMEOUT 2
// Write went out but the device never responded
#define PS2_UART_E_WRITE_RESPONSE 3
// Device responded with 0xfe (resend) and retries ran out
#define PS2_UART_E_WRITE_RESEND 4
// Device responded with 0xfc (failure / cancel)
#define PS2_UART_E_WRITE_FAILURE 5

K_MUTEX_DEFINE(ps2_uart_write_mutex);

static int ps2_uart_write_byte(uint8_t byte) {
    int err;

    LOG_DBG("Writing: 0x%x", byte);

    k_mutex_lock(&ps2_uart_write_mutex, K_FOREVER);

    for (int i = 0; i < PS2_UART_WRITE_MAX_RETRY; i++) {
        if (i > 0) {
            LOG_WRN("Attempting write re-try #%d of %d...", i + 1, PS2_UART_WRITE_MAX_RETRY);
        }

        err = ps2_uart_write_byte_await_response(byte);

        if (err == 0) {
            if (i > 0) {
                LOG_WRN("Successfully wrote 0x%x on try #%d of %d...", byte, i + 1,
                        PS2_UART_WRITE_MAX_RETRY);
            }
            break;
        } else if (err == PS2_UART_E_WRITE_FAILURE) {
            // Device requested to stop resending
            break;
        }
    }

    LOG_DBG("END WRITE: 0x%x", byte);
    k_mutex_unlock(&ps2_uart_write_mutex);

    return err;
}

// Writes the byte and blocks until the response byte arrives.
// 0xfe (resend) and 0xfc (failure) fail the write; anything else,
// including but not limited to 0xfa (ack), is success.
static int ps2_uart_write_byte_await_response(uint8_t byte) {
    struct ps2_uart_data *data = &ps2_uart_data;
    int err;

    err = ps2_uart_write_byte_blocking(byte);
    if (err) {
        return err;
    }

    data->write_awaits_resp = true;

    err = k_sem_take(&data->write_awaits_resp_sem, PS2_UART_TIMEOUT_WRITE_AWAIT_RESPONSE);

    uint8_t resp_byte = data->write_awaits_resp_byte;
    data->write_awaits_resp_byte = 0x0;
    data->write_awaits_resp = false;

    if (err) {
        LOG_WRN("Write response didn't arrive in time for byte 0x%x. "
                "Considering send a failure.",
                byte);

        return PS2_UART_E_WRITE_RESPONSE;
    }

    if (resp_byte == PS2_UART_RESP_RESEND || resp_byte == PS2_UART_RESP_FAILURE) {
        LOG_WRN("Write of 0x%x received error response: 0x%x", byte, resp_byte);
    } else {
        LOG_DBG("Write for byte 0x%x received response: 0x%x", byte, resp_byte);
    }

    if (resp_byte == PS2_UART_RESP_RESEND) {
        return PS2_UART_E_WRITE_RESEND;
    } else if (resp_byte == PS2_UART_RESP_FAILURE) {
        return PS2_UART_E_WRITE_FAILURE;
    }

    return 0;
}

static int ps2_uart_write_byte_blocking(uint8_t byte) {
    struct ps2_uart_data *data = &ps2_uart_data;
    int err;

    err = ps2_uart_write_byte_start(byte);
    if (err) {
        LOG_ERR("Could not initiate writing of byte.");
        return PS2_UART_E_WRITE_TRANSMIT;
    }

    // write_byte_start takes the only available semaphore, so this blocks
    // until ps2_uart_write_finish gives it back.
    err = k_sem_take(&data->write_lock, PS2_UART_TIMEOUT_WRITE_BLOCKING);
    if (err) {
        LOG_ERR("Blocking write failed due to semaphore timeout for byte 0x%x: %d", byte, err);

        return PS2_UART_E_WRITE_SEM_TIMEOUT;
    }

    if (data->cur_write_status == PS2_UART_WRITE_STATUS_SUCCESS) {
        err = 0;
    } else {
        LOG_ERR("Blocking write finished with failure for byte 0x%x status: %d", byte,
                data->cur_write_status);
        err = -data->cur_write_status;
    }

    data->cur_write_status = PS2_UART_WRITE_STATUS_INACTIVE;

    return err;
}

static int ps2_uart_write_byte_start(uint8_t byte) {
    struct ps2_uart_data *data = &ps2_uart_data;
    int err;

    // Take the semaphore so that ps2_uart_write_byte_blocking blocks
    // when it tries to take it.
    err = k_sem_take(&data->write_lock, K_NO_WAIT);
    if (err != 0 && err != -EBUSY) {
        LOG_ERR("ps2_uart_write_byte_start could not take semaphore: %d", err);

        return err;
    }

    err = ps2_uart_set_mode_write();
    if (err != 0) {
        LOG_ERR("Could not configure driver for write mode: %d", err);
        return err;
    }

    data->cur_write_byte = byte;
    data->cur_write_pos = PS2_UART_POS_START;

    // Inhibit the line: clock low, data high
    ps2_uart_set_scl(0);
    ps2_uart_set_sda(1);
    k_busy_wait(PS2_UART_TIMING_SCL_INHIBITION);

    // Send the start bit
    ps2_uart_set_sda(0);
    k_busy_wait(PS2_UART_TIMING_SCL_INHIBITION);

    // The next SCL interrupt is for the first data bit
    data->cur_write_pos += 1;

    // Release the clock line so the device takes over clocking
    ps2_uart_set_scl(1);
    ps2_uart_configure_pin_scl_input();

    // Execution continues in the SCL interrupt handler
    ps2_uart_set_scl_callback_enabled(true);

    // ... unless the device never starts clocking
    k_work_schedule_for_queue(&ps2_uart_work_queue, &data->write_scl_timeout,
                              PS2_UART_TIMEOUT_WRITE_SCL_START);

    return 0;
}

static void ps2_uart_write_scl_timeout(struct k_work *item) {
    ps2_uart_write_finish(false, "scl timeout");
}

#if IS_ENABLED(CONFIG_ZMK_TRACKPOINT_PS2_UART_WRITE_MODE_BLOCKING)

// Wait for the first clock edge, then clock the remaining bits out with
// busy-waits at the known cycle length instead of per-edge interrupts.
static void ps2_uart_write_scl_interrupt_handler(const struct device *dev,
                                                 struct gpio_callback *cb, uint32_t pins) {
    struct ps2_uart_data *data = &ps2_uart_data;

    k_work_cancel_delayable(&data->write_scl_timeout);

    ps2_uart_set_scl_callback_enabled(false);

    for (int i = PS2_UART_POS_DATA_FIRST; i <= PS2_UART_POS_STOP; i++) {

        if (i >= PS2_UART_POS_DATA_FIRST && i <= PS2_UART_POS_DATA_LAST) {

            int data_pos = i - PS2_UART_POS_DATA_FIRST;
            bool data_bit = PS2_UART_GET_BIT(data->cur_write_byte, data_pos);

            ps2_uart_set_sda(data_bit);
        } else if (i == PS2_UART_POS_PARITY) {

            bool byte_parity = ps2_uart_get_byte_parity(data->cur_write_byte);

            ps2_uart_set_sda(byte_parity);
        } else if (i == PS2_UART_POS_STOP) {

            ps2_uart_set_sda(1);

            // Give the data pin back to the device to receive the ack bit
            ps2_uart_configure_pin_sda_input();
        }

        k_busy_wait(PS2_UART_TIMING_SCL_CYCLE_LEN);
    }

    int ack_val = ps2_uart_get_sda();

    if (ack_val == 0) {
        ps2_uart_write_finish(true, "successful ack");
    } else {
        LOG_WRN("Ack bit was invalid for write of 0x%x", data->cur_write_byte);
        ps2_uart_write_finish(true, "failed ack");
    }
}

#else

// Set the next data bit on each falling clock edge.
static void ps2_uart_write_scl_interrupt_handler(const struct device *dev,
                                                 struct gpio_callback *cb, uint32_t pins) {
    struct ps2_uart_data *data = &ps2_uart_data;

    k_work_cancel_delayable(&data->write_scl_timeout);

    if (data->cur_write_pos == PS2_UART_POS_START) {
        // The start bit was sent during inhibition in write_byte_start
        return;
    } else if (data->cur_write_pos >= PS2_UART_POS_DATA_FIRST &&
               data->cur_write_pos <= PS2_UART_POS_DATA_LAST) {

        int data_pos = data->cur_write_pos - PS2_UART_POS_DATA_FIRST;
        bool data_bit = PS2_UART_GET_BIT(data->cur_write_byte, data_pos);

        ps2_uart_set_sda(data_bit);
    } else if (data->cur_write_pos == PS2_UART_POS_PARITY) {

        bool byte_parity = ps2_uart_get_byte_parity(data->cur_write_byte);

        ps2_uart_set_sda(byte_parity);
    } else if (data->cur_write_pos == PS2_UART_POS_STOP) {

        ps2_uart_set_sda(1);

        // Give the data pin back to the device to receive the ack bit
        ps2_uart_configure_pin_sda_input();
    } else if (data->cur_write_pos == PS2_UART_POS_ACK) {

        int ack_val = ps2_uart_get_sda();

        if (ack_val == 0) {
            ps2_uart_write_finish(true, "successful ack");
        } else {
            LOG_WRN("Ack bit was invalid for write of 0x%x", data->cur_write_byte);
            ps2_uart_write_finish(true, "failed ack");
        }
    } else {
        LOG_ERR("UART unknown TX bit number: %d", data->cur_write_pos);
    }

    if (data->cur_write_pos < PS2_UART_POS_ACK) {
        k_work_schedule_for_queue(&ps2_uart_work_queue, &data->write_scl_timeout,
                                  PS2_UART_TIMEOUT_WRITE_SCL);
    }

    data->cur_write_pos += 1;
}

#endif /* IS_ENABLED(CONFIG_ZMK_TRACKPOINT_PS2_UART_WRITE_MODE_BLOCKING) */

static void ps2_uart_write_finish(bool successful, char *descr) {
    struct ps2_uart_data *data = &ps2_uart_data;
    int err;

    k_work_cancel_delayable(&data->write_scl_timeout);

    if (successful) {
        LOG_DBG("Successfully wrote value 0x%x", data->cur_write_byte);
        data->cur_write_status = PS2_UART_WRITE_STATUS_SUCCESS;
    } else {
        LOG_ERR("Failed to write value 0x%x: %s", data->cur_write_byte, descr);

        data->cur_write_status = PS2_UART_WRITE_STATUS_FAILURE;
    }

    err = ps2_uart_set_mode_read();
    if (err != 0) {
        LOG_ERR("Could not configure driver for read mode: %d", err);
        return;
    }

    data->cur_write_byte = 0x0;

    // Unblock write_byte_blocking
    k_sem_give(&data->write_lock);
}

/*
 * Zephyr PS/2 driver API
 */

static int ps2_uart_enable_callback(const struct device *dev);

static int ps2_uart_configure(const struct device *dev, ps2_callback_t callback_isr) {
    struct ps2_uart_data *data = dev->data;

    if (!callback_isr) {
        return -EINVAL;
    }

    data->callback_isr = callback_isr;
    ps2_uart_enable_callback(dev);

    return 0;
}

static int ps2_uart_read(const struct device *dev, uint8_t *value) {
    uint8_t queue_byte;
    int err = ps2_uart_data_queue_get_next(&queue_byte, PS2_UART_TIMEOUT_READ);
    if (err) {
        return -ETIMEDOUT;
    }

    *value = queue_byte;

    return 0;
}

static int ps2_uart_write(const struct device *dev, uint8_t value) {
    return ps2_uart_write_byte(value);
}

static int ps2_uart_disable_callback(const struct device *dev) {
    struct ps2_uart_data *data = dev->data;

    // Drop stale bytes queued before the callback was disabled
    ps2_uart_data_queue_empty();

    data->callback_enabled = false;

    return 0;
}

static int ps2_uart_enable_callback(const struct device *dev) {
    struct ps2_uart_data *data = dev->data;

    data->callback_enabled = true;

    ps2_uart_data_queue_empty();

    return 0;
}

static const struct ps2_driver_api ps2_uart_driver_api = {
    .config = ps2_uart_configure,
    .read = ps2_uart_read,
    .write = ps2_uart_write,
    .disable_callback = ps2_uart_disable_callback,
    .enable_callback = ps2_uart_enable_callback,
};

/*
 * Init
 */

static int ps2_uart_init_uart(void) {
    struct ps2_uart_data *data = &ps2_uart_data;
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    if (!device_is_ready(config->uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    struct uart_config uart_cfg;
    err = uart_config_get(config->uart_dev, &uart_cfg);
    if (err != 0) {
        LOG_ERR("Could not retrieve UART config...");
        return -ENODEV;
    }

    uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
    uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;

    // PS/2 uses odd parity, which the nrf52 doesn't support. Even parity
    // turns the parity error into a validity check (see read_err_check).
    uart_cfg.parity = UART_CFG_PARITY_EVEN;

    err = uart_configure(config->uart_dev, &uart_cfg);
    if (err != 0) {
        LOG_ERR("Could not configure UART device: %d", err);
        return -EINVAL;
    }

    uart_irq_callback_user_data_set(config->uart_dev, ps2_uart_interrupt_handler,
                                    (void *)data->dev);

    uart_irq_rx_enable(config->uart_dev);
    uart_irq_err_enable(config->uart_dev);

    return 0;
}

static int ps2_uart_init_gpio(void) {
    struct ps2_uart_data *data = &ps2_uart_data;
    const struct ps2_uart_config *config = &ps2_uart_config;
    int err;

    gpio_init_callback(&data->scl_cb_data, ps2_uart_write_scl_interrupt_handler,
                       BIT(config->scl_gpio.pin));

    err = gpio_add_callback(config->scl_gpio.port, &data->scl_cb_data);
    if (err) {
        LOG_ERR("failed to enable interrupt callback on SCL GPIO pin (err %d)", err);
    }

    ps2_uart_set_scl_callback_enabled(false);

    return err;
}

static int ps2_uart_init(const struct device *dev) {
    int err;
    struct ps2_uart_data *data = dev->data;

    // Save the device struct to pass it to the ps2 callback
    data->dev = dev;

    LOG_INF("Initializing PS/2 UART transport... SCL: pin %d; SDA: pin %d",
            ps2_uart_config.scl_gpio.pin, ps2_uart_config.sda_gpio.pin);

    k_msgq_init(&data->data_queue, data->data_queue_buffer, sizeof(struct ps2_uart_data_queue_item),
                PS2_UART_DATA_QUEUE_SIZE);

    k_work_queue_start(&ps2_uart_work_queue, ps2_uart_work_queue_stack_area,
                       K_THREAD_STACK_SIZEOF(ps2_uart_work_queue_stack_area),
                       PS2_UART_WORK_QUEUE_PRIORITY, NULL);

    k_work_queue_start(&ps2_uart_work_queue_cb, ps2_uart_work_queue_cb_stack_area,
                       K_THREAD_STACK_SIZEOF(ps2_uart_work_queue_cb_stack_area),
                       PS2_UART_WORK_QUEUE_CB_PRIORITY, NULL);

    k_work_init(&data->callback_work, ps2_uart_read_callback_work_handler);
    k_work_init_delayable(&data->write_scl_timeout, ps2_uart_write_scl_timeout);

    k_sem_init(&data->write_lock, 0, 1);
    k_sem_init(&data->write_awaits_resp_sem, 0, 1);

    err = ps2_uart_init_uart();
    if (err != 0) {
        LOG_ERR("Could not init UART: %d", err);
        return err;
    }

    err = ps2_uart_init_gpio();
    if (err != 0) {
        LOG_ERR("Could not init GPIO: %d", err);
        return err;
    }

    err = ps2_uart_set_mode_read();
    if (err != 0) {
        LOG_ERR("Could not initialize in UART mode read: %d", err);
        return err;
    }

    return 0;
}

DEVICE_DT_INST_DEFINE(0, &ps2_uart_init, NULL, &ps2_uart_data, &ps2_uart_config, POST_KERNEL, 80,
                      &ps2_uart_driver_api);
