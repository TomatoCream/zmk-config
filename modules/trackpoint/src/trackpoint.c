/*
 * PS/2 TrackPoint driver.
 *
 * Initializes the device over a PS/2 transport (reset, Intellimouse scroll
 * mode, TrackPoint tuning registers) and parses movement packets into
 * standard Zephyr input events, which ZMK 0.3's native zmk,input-listener
 * turns into HID mouse reports. No ZMK APIs are called here.
 *
 * Protocol reference: IBM TrackPoint System Version 4.0 Engineering
 * Specification, and infused-kim/kb_zmk_ps2_mouse_trackpoint_driver
 * (input_mouse_ps2.c).
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_trackpoint_ps2

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/ps2.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(trackpoint, CONFIG_ZMK_TRACKPOINT_LOG_LEVEL);

/*
 * Settings
 */

// Delay driver init to give the device time to boot and send its
// power-on self-test result.
#define TP_INIT_THREAD_DELAY_MS 1000

// How often we try to initialize the device before giving up.
#define TP_INIT_ATTEMPTS 10

// Activity packets are 3-4 bytes; if this much time passes between bytes,
// give up on the packet and start fresh (also resyncs alignment).
#define TP_TIMEOUT_ACTIVITY_PACKET K_MSEC(500)

/*
 * PS/2 commands
 */

// "The POR shall be timed to occur 600 ms ± 20 % from the time power is
//  applied to the TrackPoint controller."
#define TP_POWER_ON_RESET_TIME K_MSEC(600)

#define PS2_CMD_GET_DEVICE_ID "\xf2"
#define PS2_CMD_SET_SAMPLING_RATE "\xf3"
#define PS2_CMD_ENABLE_REPORTING "\xf4"
#define PS2_CMD_DISABLE_REPORTING "\xf5"
#define PS2_CMD_RESEND "\xfe"
#define PS2_CMD_RESET "\xff"

#define PS2_SAMPLING_RATE_DEFAULT 100

// TrackPoint-specific commands, from the IBM TrackPoint System Version 4.0
// Engineering Specification (YKT3Eext.pdf)
#define TP_CMD_GET_SECONDARY_ID "\xe1"
#define TP_CMD_GET_ROM_ID "\xe2\x46"
#define TP_CMD_GET_CONFIG_BYTE "\xe2\x80\x2c"
#define TP_CMD_SET_CONFIG_BYTE "\xe2\x81\x2c"
#define TP_CMD_SET_SENSITIVITY "\xe2\x81\x4a"
#define TP_CMD_SET_NEG_INERTIA "\xe2\x81\x4d"
#define TP_CMD_SET_VALUE6_UPPER_PLATEAU_SPEED "\xe2\x81\x60"
#define TP_CMD_SET_PTS_THRESHOLD "\xe2\x81\x5c"

// TrackPoint config byte bits
#define TP_CONFIG_BIT_PRESS_TO_SELECT 0x00
#define TP_CONFIG_BIT_INVERT_X 0x03
#define TP_CONFIG_BIT_INVERT_Y 0x04
#define TP_CONFIG_BIT_INVERT_Z 0x05
#define TP_CONFIG_BIT_SWAP_XY 0x06

#define PS2_RESP_SELF_TEST_PASS 0xaa

#define TP_THREAD_STACK_SIZE 2048
#define TP_THREAD_PRIORITY 10

typedef enum {
    TP_PACKET_MODE_PS2_DEFAULT,
    TP_PACKET_MODE_SCROLL,
} trackpoint_packet_mode;

struct trackpoint_config {
    const struct device *ps2_device;
    struct gpio_dt_spec rst_gpio;

    bool scroll_mode;
    bool disable_clicking;
    int sampling_rate;

    bool tp_press_to_select;
    int tp_press_to_select_threshold;
    int tp_sensitivity;
    int tp_neg_inertia;
    int tp_val6_upper_speed;
    bool tp_x_invert;
    bool tp_y_invert;
    bool tp_xy_swap;
};

struct trackpoint_packet {
    int16_t mov_x;
    int16_t mov_y;
    int8_t scroll;
    bool overflow_x;
    bool overflow_y;
    bool button_l;
    bool button_m;
    bool button_r;
};

struct trackpoint_data {
    const struct device *dev;

    K_THREAD_STACK_MEMBER(thread_stack, TP_THREAD_STACK_SIZE);
    struct k_thread thread;

    trackpoint_packet_mode packet_mode;
    uint8_t packet_buffer[4];
    int packet_idx;
    struct trackpoint_packet prev_packet;
    struct k_work_delayable packet_buffer_timeout;

    bool button_l_is_held;
    bool button_m_is_held;
    bool button_r_is_held;

    bool activity_reporting_on;
    bool is_trackpoint;
    uint8_t manufacturer_id;
    uint8_t secondary_id;
    uint8_t rom_id;

    uint8_t sampling_rate;
};

static const struct trackpoint_config trackpoint_config = {
    .ps2_device = DEVICE_DT_GET(DT_INST_PHANDLE(0, ps2_device)),

#if DT_INST_NODE_HAS_PROP(0, rst_gpios)
    .rst_gpio = GPIO_DT_SPEC_INST_GET(0, rst_gpios),
#else
    .rst_gpio = {.port = NULL, .pin = 0, .dt_flags = 0},
#endif

    .scroll_mode = DT_INST_PROP_OR(0, scroll_mode, false),
    .disable_clicking = DT_INST_PROP_OR(0, disable_clicking, false),
    .sampling_rate = DT_INST_PROP_OR(0, sampling_rate, PS2_SAMPLING_RATE_DEFAULT),
    .tp_press_to_select = DT_INST_PROP_OR(0, tp_press_to_select, false),
    .tp_press_to_select_threshold = DT_INST_PROP_OR(0, tp_press_to_select_threshold, -1),
    .tp_sensitivity = DT_INST_PROP_OR(0, tp_sensitivity, -1),
    .tp_neg_inertia = DT_INST_PROP_OR(0, tp_neg_inertia, -1),
    .tp_val6_upper_speed = DT_INST_PROP_OR(0, tp_val6_upper_speed, -1),
    .tp_x_invert = DT_INST_PROP_OR(0, tp_x_invert, false),
    .tp_y_invert = DT_INST_PROP_OR(0, tp_y_invert, false),
    .tp_xy_swap = DT_INST_PROP_OR(0, tp_xy_swap, false),
};

static struct trackpoint_data trackpoint_data = {
    .packet_mode = TP_PACKET_MODE_PS2_DEFAULT,
    .packet_idx = 0,

    .button_l_is_held = false,
    .button_m_is_held = false,
    .button_r_is_held = false,

    .activity_reporting_on = false,

    .is_trackpoint = false,
    .manufacturer_id = 0x0,
    .secondary_id = 0x0,
    .rom_id = 0x0,

    .sampling_rate = PS2_SAMPLING_RATE_DEFAULT,
};

static const int allowed_sampling_rates[] = {10, 20, 40, 60, 80, 100, 200};

#define TP_GET_BIT(data, bit_pos) (((data) >> (bit_pos)) & 0x1)
#define TP_SET_BIT(data, bit_val, bit_pos) ((data) |= (bit_val) << (bit_pos))

static int trackpoint_activity_reporting_enable(void);
static int trackpoint_activity_reporting_disable(void);

/*
 * Packet reading
 */

static void trackpoint_activity_reset_packet_buffer(void) {
    struct trackpoint_data *data = &trackpoint_data;

    data->packet_idx = 0;
    memset(data->packet_buffer, 0x0, sizeof(data->packet_buffer));
}

static void trackpoint_activity_abort_cmd(char *reason) {
    struct trackpoint_data *data = &trackpoint_data;
    const struct trackpoint_config *config = &trackpoint_config;

    LOG_WRN("PS/2 packet buffer out of alignment. Requesting resend: %s", reason);

    data->packet_idx = 0;
    ps2_write(config->ps2_device, PS2_CMD_RESEND[0]);

    trackpoint_activity_reset_packet_buffer();
}

static struct trackpoint_packet trackpoint_activity_parse_packet_buffer(
    trackpoint_packet_mode packet_mode, uint8_t packet_state, uint8_t packet_x, uint8_t packet_y,
    uint8_t packet_extra) {
    struct trackpoint_packet packet;

    packet.button_l = TP_GET_BIT(packet_state, 0);
    packet.button_r = TP_GET_BIT(packet_state, 1);
    packet.button_m = TP_GET_BIT(packet_state, 2);
    packet.overflow_x = TP_GET_BIT(packet_state, 6);
    packet.overflow_y = TP_GET_BIT(packet_state, 7);
    packet.scroll = 0;

    // Movement is a signed 9-bit value whose sign bit lives in the state
    // byte. Conversion trick from https://wiki.osdev.org/PS/2_Mouse
    packet.mov_x = packet_x - ((packet_state << 4) & 0x100);
    packet.mov_y = packet_y - ((packet_state << 3) & 0x100);

    // In scroll mode the 4th byte is the signed scroll wheel movement.
    if (packet_mode == TP_PACKET_MODE_SCROLL) {
        packet.scroll = (int8_t)packet_extra;
    }

    return packet;
}

static void trackpoint_activity_move_mouse(struct trackpoint_packet *packet, bool buttons_follow) {
    struct trackpoint_data *data = &trackpoint_data;

    bool have_x = packet->mov_x != 0;
    bool have_y = packet->mov_y != 0;
    bool have_scroll = packet->scroll != 0;

    if (have_x) {
        bool sync = !have_y && !have_scroll && !buttons_follow;
        input_report_rel(data->dev, INPUT_REL_X, packet->mov_x, sync, K_NO_WAIT);
    }
    if (have_y) {
        bool sync = !have_scroll && !buttons_follow;
        input_report_rel(data->dev, INPUT_REL_Y, packet->mov_y, sync, K_NO_WAIT);
    }
    if (have_scroll) {
        input_report_rel(data->dev, INPUT_REL_WHEEL, packet->scroll, !buttons_follow, K_NO_WAIT);
    }
}

static void trackpoint_activity_click_buttons(bool button_l, bool button_m, bool button_r) {
    struct trackpoint_data *data = &trackpoint_data;
    const struct trackpoint_config *config = &trackpoint_config;

    int buttons_pressed = 0;
    int buttons_released = 0;

    bool button_l_pressed = false;
    bool button_l_released = false;
    if (button_l == true && data->button_l_is_held == false) {
        button_l_pressed = true;
        buttons_pressed++;
    } else if (button_l == false && data->button_l_is_held == true) {
        button_l_released = true;
        buttons_released++;
    }

    bool button_m_released = false;
    bool button_m_pressed = false;
    if (button_m == true && data->button_m_is_held == false) {
        button_m_pressed = true;
        buttons_pressed++;
    } else if (button_m == false && data->button_m_is_held == true) {
        button_m_released = true;
        buttons_released++;
    }

    bool button_r_released = false;
    bool button_r_pressed = false;
    if (button_r == true && data->button_r_is_held == false) {
        button_r_pressed = true;
        buttons_pressed++;
    } else if (button_r == false && data->button_r_is_held == true) {
        button_r_released = true;
        buttons_released++;
    }

#if IS_ENABLED(CONFIG_ZMK_TRACKPOINT_ERROR_MITIGATION)
    // Multiple simultaneous state changes in one packet are almost always
    // a transmission error.
    if (buttons_pressed > 1 || buttons_released > 1) {
        LOG_WRN("Ignoring button presses: Received %d presses and %d releases in one packet. "
                "Probably a transmission error.",
                buttons_pressed, buttons_released);

        trackpoint_activity_abort_cmd("Multiple button presses");
        return;
    }
#endif

    if (config->disable_clicking) {
        return;
    }

    int buttons_need_reporting = buttons_pressed + buttons_released;
    if (buttons_need_reporting == 0) {
        return;
    }

    if (button_l_pressed || button_l_released) {
        LOG_INF("%s button_l", button_l_pressed ? "Pressing" : "Releasing");
        input_report_key(data->dev, INPUT_BTN_0, button_l_pressed ? 1 : 0,
                         buttons_need_reporting == 1, K_FOREVER);
        data->button_l_is_held = button_l_pressed;
        buttons_need_reporting--;
    }

    if (buttons_need_reporting > 0 && (button_r_pressed || button_r_released)) {
        LOG_INF("%s button_r", button_r_pressed ? "Pressing" : "Releasing");
        input_report_key(data->dev, INPUT_BTN_1, button_r_pressed ? 1 : 0,
                         buttons_need_reporting == 1, K_FOREVER);
        data->button_r_is_held = button_r_pressed;
        buttons_need_reporting--;
    }

    if (buttons_need_reporting > 0 && (button_m_pressed || button_m_released)) {
        LOG_INF("%s button_m", button_m_pressed ? "Pressing" : "Releasing");
        input_report_key(data->dev, INPUT_BTN_2, button_m_pressed ? 1 : 0, true, K_FOREVER);
        data->button_m_is_held = button_m_pressed;
    }
}

static void trackpoint_activity_process_cmd(trackpoint_packet_mode packet_mode,
                                            uint8_t packet_state, uint8_t packet_x,
                                            uint8_t packet_y, uint8_t packet_extra) {
    struct trackpoint_data *data = &trackpoint_data;
    struct trackpoint_packet packet;
    packet = trackpoint_activity_parse_packet_buffer(packet_mode, packet_state, packet_x, packet_y,
                                                     packet_extra);

    LOG_DBG("Got activity packet "
            "(mov_x=%d, mov_y=%d, o_x=%d, o_y=%d, scroll=%d, b_l=%d, b_m=%d, b_r=%d)",
            packet.mov_x, packet.mov_y, packet.overflow_x, packet.overflow_y, packet.scroll,
            packet.button_l, packet.button_m, packet.button_r);

#if IS_ENABLED(CONFIG_ZMK_TRACKPOINT_ERROR_MITIGATION)
    int x_delta = abs(data->prev_packet.mov_x - packet.mov_x);
    int y_delta = abs(data->prev_packet.mov_y - packet.mov_y);

    if (packet.overflow_x == 1 && packet.overflow_y == 1) {
        LOG_WRN("Detected overflow in both x and y. Probably mistransmission. Aborting...");

        trackpoint_activity_abort_cmd("Overflow in both x and y");
        return;
    }

    // Movement beyond this threshold is likely a mistransmission or
    // misalignment; only checked when there was recent prior movement.
    if ((packet.mov_x != 0 && packet.mov_y != 0) && (x_delta > 150 || y_delta > 150)) {
        LOG_WRN("Detected malformed packet with "
                "(mov_x=%d, mov_y=%d, x_delta=%d, y_delta=%d)",
                packet.mov_x, packet.mov_y, x_delta, y_delta);
        trackpoint_activity_abort_cmd("Exceeds movement threshold.");
        return;
    }
#endif

    bool buttons_changed = packet.button_l != data->button_l_is_held ||
                           packet.button_m != data->button_m_is_held ||
                           packet.button_r != data->button_r_is_held;

    trackpoint_activity_move_mouse(&packet, buttons_changed);
    trackpoint_activity_click_buttons(packet.button_l, packet.button_m, packet.button_r);

    data->prev_packet = packet;
}

// Called by the PS/2 transport whenever the device sends a byte while
// reporting is enabled.
static void trackpoint_activity_callback(const struct device *ps2_device, uint8_t byte) {
    struct trackpoint_data *data = &trackpoint_data;

    k_work_cancel_delayable(&data->packet_buffer_timeout);

    data->packet_buffer[data->packet_idx] = byte;

    if (data->packet_idx == 0) {
        // Bit 3 of the first byte is always 1. If it's not, we are out of
        // alignment and ask for the whole packet again.
        if (TP_GET_BIT(byte, 3) != 1) {
            trackpoint_activity_abort_cmd("Bit 3 of packet is 0 instead of 1");
            return;
        }
    } else if ((data->packet_mode == TP_PACKET_MODE_PS2_DEFAULT && data->packet_idx == 2) ||
               (data->packet_mode == TP_PACKET_MODE_SCROLL && data->packet_idx == 3)) {

        trackpoint_activity_process_cmd(data->packet_mode, data->packet_buffer[0],
                                        data->packet_buffer[1], data->packet_buffer[2],
                                        data->packet_buffer[3]);
        trackpoint_activity_reset_packet_buffer();
        return;
    }

    data->packet_idx += 1;

    k_work_schedule(&data->packet_buffer_timeout, TP_TIMEOUT_ACTIVITY_PACKET);
}

// Called when no byte arrives within TP_TIMEOUT_ACTIVITY_PACKET; resyncs
// alignment after transmission hiccups.
static void trackpoint_activity_packet_timeout(struct k_work *item) {
    struct trackpoint_data *data = &trackpoint_data;

    LOG_DBG("Activity packet timed out on idx=%d", data->packet_idx);

    trackpoint_activity_reset_packet_buffer();
}

/*
 * PS/2 command sending
 */

struct trackpoint_send_cmd_resp {
    int err;
    char err_msg[80];
    uint8_t resp_buffer[8];
    int resp_len;
};

static struct trackpoint_send_cmd_resp trackpoint_send_cmd(const char *cmd, int cmd_len,
                                                           const uint8_t *arg, int resp_len,
                                                           bool pause_reporting) {
    struct trackpoint_data *data = &trackpoint_data;
    const struct trackpoint_config *config = &trackpoint_config;
    const struct device *ps2_device = config->ps2_device;
    int err = 0;
    bool prev_activity_reporting_on = data->activity_reporting_on;

    struct trackpoint_send_cmd_resp resp = {
        .err = 0,
        .err_msg = "",
        .resp_len = 0,
    };
    memset(resp.resp_buffer, 0x0, sizeof(resp.resp_buffer));

    // cmd is a string literal; don't send the terminating NULL byte
    int cmd_bytes = cmd_len - 1;
    if (cmd_bytes < 1) {
        resp.err = -10;
        snprintf(resp.err_msg, sizeof(resp.err_msg),
                 "Cannot send cmd with less than 1 byte length");

        return resp;
    }

    if (resp_len > sizeof(resp.resp_buffer)) {
        resp.err = -11;
        snprintf(resp.err_msg, sizeof(resp.err_msg),
                 "Response can't be longer than the resp_buffer (%d)", sizeof(resp.err_msg));

        return resp;
    }

    if (pause_reporting == true && data->activity_reporting_on == true) {
        LOG_DBG("Disabling activity reporting...");

        resp.err = trackpoint_activity_reporting_disable();
        if (resp.err) {
            snprintf(resp.err_msg, sizeof(resp.err_msg), "Could not disable data reporting (%d)",
                     err);
        }
    }

    if (resp.err == 0) {
        for (int i = 0; i < cmd_bytes; i++) {
            resp.err = ps2_write(ps2_device, cmd[i]);
            if (resp.err) {
                snprintf(resp.err_msg, sizeof(resp.err_msg), "Could not send cmd byte %d/%d (%d)",
                         i + 1, cmd_bytes, err);
                break;
            }
        }
    }

    if (resp.err == 0 && arg != NULL) {
        resp.err = ps2_write(ps2_device, *arg);
        if (resp.err) {
            snprintf(resp.err_msg, sizeof(resp.err_msg), "Could not send arg (%d)", err);
        }
    }

    if (resp.err == 0 && resp_len > 0) {
        for (int i = 0; i < resp_len; i++) {
            resp.err = ps2_read(ps2_device, &resp.resp_buffer[i]);
            if (resp.err) {
                snprintf(resp.err_msg, sizeof(resp.err_msg),
                         "Could not read response cmd byte %d/%d (%d)", i + 1, resp_len, err);
                break;
            }
        }
    }

    if (pause_reporting == true && prev_activity_reporting_on == true) {
        LOG_DBG("Re-enabling activity reporting...");

        err = trackpoint_activity_reporting_enable();
        if (err && resp.err == 0) {
            resp.err = err;
            snprintf(resp.err_msg, sizeof(resp.err_msg), "Could not re-enable data reporting (%d)",
                     err);
        }
    }

    return resp;
}

static int trackpoint_activity_reporting_enable(void) {
    struct trackpoint_data *data = &trackpoint_data;
    const struct trackpoint_config *config = &trackpoint_config;
    const struct device *ps2_device = config->ps2_device;

    if (data->activity_reporting_on == true) {
        return 0;
    }

    int err = ps2_write(ps2_device, PS2_CMD_ENABLE_REPORTING[0]);
    if (err) {
        LOG_ERR("Could not enable data reporting: %d", err);
        return err;
    }

    err = ps2_enable_callback(ps2_device);
    if (err) {
        LOG_ERR("Could not enable ps2 callback: %d", err);
        return err;
    }

    data->activity_reporting_on = true;

    return 0;
}

static int trackpoint_activity_reporting_disable(void) {
    struct trackpoint_data *data = &trackpoint_data;
    const struct trackpoint_config *config = &trackpoint_config;
    const struct device *ps2_device = config->ps2_device;

    if (data->activity_reporting_on == false) {
        return 0;
    }

    int err = ps2_write(ps2_device, PS2_CMD_DISABLE_REPORTING[0]);
    if (err) {
        LOG_ERR("Could not disable data reporting: %d", err);
        return err;
    }

    err = ps2_disable_callback(ps2_device);
    if (err) {
        LOG_ERR("Could not disable ps2 callback: %d", err);
        return err;
    }

    data->activity_reporting_on = false;

    return 0;
}

/*
 * PS/2 commands
 */

static int trackpoint_reset(void) {
    struct trackpoint_send_cmd_resp resp =
        trackpoint_send_cmd(PS2_CMD_RESET, sizeof(PS2_CMD_RESET), NULL, 0, false);
    if (resp.err) {
        LOG_ERR("Could not send reset cmd: %s", resp.err_msg);
    }

    return resp.err;
}

static int trackpoint_set_sampling_rate(uint8_t sampling_rate) {
    struct trackpoint_data *data = &trackpoint_data;

    bool rate_allowed = false;
    for (int i = 0; i < ARRAY_SIZE(allowed_sampling_rates); i++) {
        if (allowed_sampling_rates[i] == sampling_rate) {
            rate_allowed = true;
            break;
        }
    }
    if (!rate_allowed) {
        LOG_ERR("Requested to set illegal sampling rate: %d", sampling_rate);
        return -EINVAL;
    }

    struct trackpoint_send_cmd_resp resp = trackpoint_send_cmd(
        PS2_CMD_SET_SAMPLING_RATE, sizeof(PS2_CMD_SET_SAMPLING_RATE), &sampling_rate, 0, true);
    if (resp.err) {
        LOG_ERR("Could not set sample rate to %d: %s", sampling_rate, resp.err_msg);
        return resp.err;
    }

    data->sampling_rate = sampling_rate;

    LOG_INF("Successfully set sampling rate to %d", sampling_rate);

    return resp.err;
}

static int trackpoint_get_device_id(uint8_t *device_id) {
    struct trackpoint_send_cmd_resp resp =
        trackpoint_send_cmd(PS2_CMD_GET_DEVICE_ID, sizeof(PS2_CMD_GET_DEVICE_ID), NULL, 1, true);
    if (resp.err) {
        LOG_ERR("Could not get device id: %s", resp.err_msg);
        return resp.err;
    }

    *device_id = resp.resp_buffer[0];

    return 0;
}

static int trackpoint_set_packet_mode(trackpoint_packet_mode mode) {
    struct trackpoint_data *data = &trackpoint_data;

    if (mode == TP_PACKET_MODE_PS2_DEFAULT) {
        // Default mode; nothing to enable.
        return 0;
    }

    bool prev_activity_reporting_on = data->activity_reporting_on;
    trackpoint_activity_reporting_disable();

    // Intellimouse scroll mode is enabled with a magic sequence of
    // sampling rates.
    if (mode == TP_PACKET_MODE_SCROLL) {
        trackpoint_set_sampling_rate(200);
        trackpoint_set_sampling_rate(100);
        trackpoint_set_sampling_rate(80);
    }

    uint8_t device_id;
    int err = trackpoint_get_device_id(&device_id);
    if (err) {
        LOG_ERR("Could not enable packet mode %d. Failed to get device id: %d", mode, err);
    } else {
        if (device_id == 0x03 || device_id == 0x04) {
            LOG_INF("Successfully activated packet mode %d. Device id: %d", mode, device_id);

            data->packet_mode = TP_PACKET_MODE_SCROLL;
            err = 0;
        } else {
            LOG_ERR("Could not enable packet mode %d. Device returned id %d", mode, device_id);

            data->packet_mode = TP_PACKET_MODE_PS2_DEFAULT;
            err = 1;
        }
    }

    // Restore the sampling rate to its previous value
    trackpoint_set_sampling_rate(data->sampling_rate);

    if (prev_activity_reporting_on == true) {
        trackpoint_activity_reporting_enable();
    }

    return err;
}

/*
 * TrackPoint commands
 */

static int trackpoint_tp_get_secondary_id(uint8_t *manufacturer_id, uint8_t *secondary_id) {
    struct trackpoint_send_cmd_resp resp = trackpoint_send_cmd(
        TP_CMD_GET_SECONDARY_ID, sizeof(TP_CMD_GET_SECONDARY_ID), NULL, 2, true);
    if (resp.err) {
        return resp.err;
    }

    *manufacturer_id = resp.resp_buffer[0];
    *secondary_id = resp.resp_buffer[1];

    return 0;
}

static int trackpoint_tp_get_rom_id(uint8_t *rom_id) {
    struct trackpoint_send_cmd_resp resp =
        trackpoint_send_cmd(TP_CMD_GET_ROM_ID, sizeof(TP_CMD_GET_ROM_ID), NULL, 1, true);
    if (resp.err) {
        return resp.err;
    }

    *rom_id = resp.resp_buffer[0];

    return 0;
}

static const char *trackpoint_get_manufacturer_str(uint8_t manufacturer_id) {
    switch (manufacturer_id) {
    case 0x1:
        return "IBM";
    case 0x2:
        return "Alps";
    case 0x3:
        return "Elan";
    case 0x4:
        return "NXP";
    case 0x5:
        return "JYT Synaptics";
    case 0x6:
        return "Synaptics";
    }

    return "Unknown";
}

// Only TrackPoints implement the secondary-id command; regular PS/2 mice
// fail it. That's how we detect what's connected.
static int trackpoint_tp_get_device_info(bool *is_tp, uint8_t *tp_manufacturer_id,
                                         uint8_t *tp_secondary_id, uint8_t *tp_rom_id,
                                         char *device_str, int device_str_size) {
    int err = trackpoint_tp_get_secondary_id(tp_manufacturer_id, tp_secondary_id);
    if (err) {
        *is_tp = false;
        *tp_manufacturer_id = 0x0;
        *tp_secondary_id = 0x0;
        *tp_rom_id = 0x0;

        snprintf(device_str, device_str_size, "Generic PS/2 Mouse");

        return 0;
    }

    *is_tp = true;

    err = trackpoint_tp_get_rom_id(tp_rom_id);
    if (err) {
        LOG_ERR("Could not determine TP rom id: %d", err);
        *tp_rom_id = 0x0;
        err = -1;
    }

    snprintf(device_str, device_str_size,
             "Trackpoint by %s (0x%02X); Secondary ID: 0x%02X; Rom Version: %02X",
             trackpoint_get_manufacturer_str(*tp_manufacturer_id), *tp_manufacturer_id,
             *tp_secondary_id, *tp_rom_id);

    return err;
}

static int trackpoint_tp_get_config_byte(uint8_t *config_byte) {
    struct trackpoint_send_cmd_resp resp =
        trackpoint_send_cmd(TP_CMD_GET_CONFIG_BYTE, sizeof(TP_CMD_GET_CONFIG_BYTE), NULL, 1, true);
    if (resp.err) {
        LOG_ERR("Could not read trackpoint config byte: %s", resp.err_msg);
        return resp.err;
    }

    *config_byte = resp.resp_buffer[0];

    return 0;
}

static int trackpoint_tp_set_config_option(int config_bit, bool enabled, char *descr) {
    uint8_t config_byte;
    int err = trackpoint_tp_get_config_byte(&config_byte);
    if (err) {
        return err;
    }

    bool is_enabled = TP_GET_BIT(config_byte, config_bit);

    if (is_enabled == enabled) {
        LOG_DBG("Trackpoint %s was already %s... not doing anything.", descr,
                is_enabled ? "enabled" : "disabled");
        return 0;
    }

    TP_SET_BIT(config_byte, enabled, config_bit);

    struct trackpoint_send_cmd_resp resp = trackpoint_send_cmd(
        TP_CMD_SET_CONFIG_BYTE, sizeof(TP_CMD_SET_CONFIG_BYTE), &config_byte, 0, true);
    if (resp.err) {
        LOG_ERR("Could not set trackpoint %s to %s: %s", descr, enabled ? "enabled" : "disabled",
                resp.err_msg);
        return resp.err;
    }

    LOG_INF("Successfully set config option %s to %s", descr, enabled ? "enabled" : "disabled");

    return 0;
}

static int trackpoint_tp_set_value(const char *cmd, int cmd_len, int value, char *descr) {
    if (value < 0 || value > 255) {
        LOG_ERR("Invalid %s value %d. Allowed range: 0-255", descr, value);
        return -EINVAL;
    }

    uint8_t arg = value;

    struct trackpoint_send_cmd_resp resp = trackpoint_send_cmd(cmd, cmd_len, &arg, 0, true);
    if (resp.err) {
        LOG_ERR("Could not set %s to %d: %s", descr, value, resp.err_msg);
        return resp.err;
    }

    LOG_INF("Successfully set TP %s to %d", descr, value);

    return 0;
}

/*
 * Init
 */

// Power-On-Reset for the optional rst-gpios pin.
// "The TrackPoint logic shall execute a Power On Reset (POR) when power is
//  applied to the device. The POR shall be timed to occur 600 ms ± 20 % from
//  the time power is applied to the TrackPoint controller."
static int trackpoint_init_power_on_reset(void) {
    const struct trackpoint_config *config = &trackpoint_config;

    if (config->rst_gpio.port == NULL) {
        return 0;
    }

    LOG_INF("Performing Power-On-Reset...");

    struct gpio_dt_spec rst_gpio = config->rst_gpio;
    rst_gpio.dt_flags = 0;

    int err = gpio_pin_configure_dt(&rst_gpio, (GPIO_OUTPUT_HIGH));
    if (err) {
        LOG_ERR("Failed Power-On-Reset: could not configure RST GPIO pin (err %d)", err);
        return err;
    }

    k_sleep(TP_POWER_ON_RESET_TIME);

    err = gpio_pin_set_dt(&rst_gpio, 0);
    if (err) {
        LOG_ERR("Failed Power-On-Reset: could not set RST GPIO pin low (err %d)", err);
        return err;
    }

    LOG_DBG("Finished Power-On-Reset successfully...");

    return 0;
}

static int trackpoint_init_wait_for_device(const struct device *dev) {
    const struct trackpoint_config *config = dev->config;
    int err;

    uint8_t read_val;

    for (int i = 0; i < TP_INIT_ATTEMPTS; i++) {

        LOG_INF("Trying to initialize trackpoint (attempt %d / %d)", i + 1, TP_INIT_ATTEMPTS);

        // PS/2 devices send the result of their self-test when they power up.
        err = ps2_read(config->ps2_device, &read_val);
        if (err == 0) {
            if (read_val != PS2_RESP_SELF_TEST_PASS) {
                LOG_WRN("Got invalid PS/2 self-test result: 0x%x", read_val);

                LOG_INF("Trying to reset PS2 device...");
                trackpoint_reset();

                continue;
            }

            LOG_INF("PS/2 device passed self-test: 0x%x", read_val);

            LOG_INF("Reading PS/2 device id...");
            err = ps2_read(config->ps2_device, &read_val);
            if (err) {
                LOG_WRN("Could not read PS/2 device id: %d", err);
            } else {
                if (read_val == 0) {
                    LOG_INF("Connected PS/2 device is a mouse...");
                    return 0;
                }

                LOG_WRN("PS/2 device is not a mouse: 0x%x", read_val);
                return 1;
            }
        } else {
            LOG_WRN("Could not read PS/2 device self-test result: %d.", err);
        }

        // Resets of the keyboard controller don't cut power to the
        // trackpoint, so it never re-sends the self-test result on a
        // firmware reset. Send an explicit reset command instead.
        if (i % 2 == 0) {
            LOG_INF("Trying to reset PS2 device...");
            trackpoint_reset();
            continue;
        }

        k_sleep(K_SECONDS(5));
    }

    return 1;
}

static void trackpoint_init_thread(int dev_ptr, int unused) {
    struct trackpoint_data *data = &trackpoint_data;
    int err;

    data->dev = INT_TO_POINTER(dev_ptr);

    const struct trackpoint_config *config = data->dev->config;

    trackpoint_init_power_on_reset();

    LOG_INF("Waiting for trackpoint to connect...");
    err = trackpoint_init_wait_for_device(data->dev);
    if (err) {
        LOG_ERR("Could not init a trackpoint in %d attempts. Giving up. "
                "Power cycle the device and reset zmk to try again.",
                TP_INIT_ATTEMPTS);
        return;
    }

    if (config->sampling_rate != PS2_SAMPLING_RATE_DEFAULT) {
        LOG_INF("Setting sample rate to %d...", config->sampling_rate);
        trackpoint_set_sampling_rate(config->sampling_rate);
    }

    char device_descr[64] = "undetermined device";
    trackpoint_tp_get_device_info(&data->is_trackpoint, &data->manufacturer_id,
                                  &data->secondary_id, &data->rom_id, device_descr,
                                  sizeof(device_descr));

    LOG_INF("Connected device is a %s", device_descr);

    if (data->is_trackpoint == true) {

        if (config->tp_press_to_select) {
            LOG_INF("Enabling TP press to select...");
            trackpoint_tp_set_config_option(TP_CONFIG_BIT_PRESS_TO_SELECT, true,
                                            "Press To Select");
        }

        if (config->tp_press_to_select_threshold != -1) {
            LOG_INF("Setting TP press to select threshold to %d...",
                    config->tp_press_to_select_threshold);
            trackpoint_tp_set_value(TP_CMD_SET_PTS_THRESHOLD, sizeof(TP_CMD_SET_PTS_THRESHOLD),
                                    config->tp_press_to_select_threshold,
                                    "press-to-select threshold");
        }

        if (config->tp_sensitivity != -1) {
            LOG_INF("Setting TP sensitivity to %d...", config->tp_sensitivity);
            trackpoint_tp_set_value(TP_CMD_SET_SENSITIVITY, sizeof(TP_CMD_SET_SENSITIVITY),
                                    config->tp_sensitivity, "sensitivity");
        }

        if (config->tp_neg_inertia != -1) {
            LOG_INF("Setting TP negative inertia to %d...", config->tp_neg_inertia);
            trackpoint_tp_set_value(TP_CMD_SET_NEG_INERTIA, sizeof(TP_CMD_SET_NEG_INERTIA),
                                    config->tp_neg_inertia, "negative inertia");
        }

        if (config->tp_val6_upper_speed != -1) {
            LOG_INF("Setting TP value6 upper plateau speed to %d...", config->tp_val6_upper_speed);
            trackpoint_tp_set_value(TP_CMD_SET_VALUE6_UPPER_PLATEAU_SPEED,
                                    sizeof(TP_CMD_SET_VALUE6_UPPER_PLATEAU_SPEED),
                                    config->tp_val6_upper_speed, "value6 upper plateau speed");
        }

        if (config->tp_x_invert) {
            LOG_INF("Inverting trackpoint x axis.");
            trackpoint_tp_set_config_option(TP_CONFIG_BIT_INVERT_X, true, "Invert X");
        }

        if (config->tp_y_invert) {
            LOG_INF("Inverting trackpoint y axis.");
            trackpoint_tp_set_config_option(TP_CONFIG_BIT_INVERT_Y, true, "Invert Y");
        }

        if (config->tp_xy_swap) {
            LOG_INF("Swapping trackpoint x and y axis.");
            trackpoint_tp_set_config_option(TP_CONFIG_BIT_SWAP_XY, true, "Swap XY");
        }
    }

    if (config->scroll_mode) {
        LOG_INF("Enabling scroll mode.");
        trackpoint_set_packet_mode(TP_PACKET_MODE_SCROLL);
    }

    k_work_init_delayable(&data->packet_buffer_timeout, trackpoint_activity_packet_timeout);

    LOG_DBG("Configuring ps2 callback...");
    err = ps2_config(config->ps2_device, &trackpoint_activity_callback);
    if (err) {
        LOG_ERR("Could not configure ps2 interface: %d", err);
        return;
    }

    LOG_INF("Enabling data reporting and ps2 callback...");
    err = trackpoint_activity_reporting_enable();
    if (err) {
        LOG_ERR("Could not activate ps2 callback: %d", err);
    } else {
        LOG_DBG("Successfully activated ps2 callback");
    }
}

static int trackpoint_init(const struct device *dev) {
    LOG_DBG("Creating trackpoint init thread.");

    k_thread_create(&trackpoint_data.thread, trackpoint_data.thread_stack, TP_THREAD_STACK_SIZE,
                    (k_thread_entry_t)trackpoint_init_thread, (struct device *)dev, 0, NULL,
                    K_PRIO_COOP(TP_THREAD_PRIORITY), 0, K_MSEC(TP_INIT_THREAD_DELAY_MS));

    return 0;
}

// Must init after the PS/2 transport (POST_KERNEL 80)
#define TP_INIT_PRIORITY 90

DEVICE_DT_INST_DEFINE(0, &trackpoint_init, NULL, &trackpoint_data, &trackpoint_config, POST_KERNEL,
                      TP_INIT_PRIORITY, NULL);
