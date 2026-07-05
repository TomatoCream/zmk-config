# ZMK 0.3 TrackPoint Driver Implementation Prompt

## Goal

Implement a PS/2 TrackPoint driver as a Zephyr input driver for ZMK 0.3, living inside
the zmk-config repo at `modules/trackpoint/`. The driver should be a standard Zephyr
input device that emits `input_report()` events, which ZMK 0.3's native
`zmk,input-listener` converts to HID mouse reports.

**Do NOT depend on or import infused-kim's ZMK fork or driver module.** Use official
ZMK 0.3 (`zmkfirmware/zmk` main branch). Reference infused-kim's implementation only
for PS/2 protocol knowledge and TrackPoint command sequences.

## Architecture Overview

```
Your driver (Zephyr input device)
    │  emits: input_report(INPUT_EV_REL, INPUT_REL_X/Y)
    │         input_report(INPUT_EV_KEY, INPUT_BTN_0/1/2)
    │         input_report_sync()
    ▼
ZMK 0.3 native zmk,input-listener  (app/src/pointing/input_listener.c)
    │  calls: zmk_hid_mouse_movement_set()
    │         zmk_hid_mouse_scroll_set()
    │         zmk_hid_mouse_button_press/release()
    │         zmk_endpoint_send_mouse_report()
    ▼
BLE HOG / USB HID  →  Host
```

Your driver does NOT call any ZMK APIs. It only uses Zephyr's input subsystem.

## Target Repo Structure

```
zmk-config/                          (this repo, branch: trackpoint-dev)
├── flake.nix
├── config/
│   ├── west.yml                     (points to zmkfirmware/zmk main, no infused-kim)
│   ├── do52pro.keymap
│   └── do52pro.conf                 (CONFIG_ZMK_POINTING=y)
├── boards/shields/do52pro/
│   ├── do52pro.dtsi
│   ├── do52pro_left.overlay
│   ├── do52pro_right.overlay        (defines trackpoint DT node + zmk,input-listener)
│   └── ...
├── modules/
│   └── trackpoint/                  ← YOUR DRIVER CODE
│       ├── zephyr/
│       │   ├── module.yml           (registers as Zephyr module)
│       │   ├── CMakeLists.txt       (adds src/ subdirectory)
│       │   └── Kconfig              (menuconfig for trackpoint driver)
│       ├── CMakeLists.txt           (top-level, delegates to zephyr/)
│       ├── Kconfig                  (top-level, delegates to zephyr/)
│       ├── src/
│       │   ├── ps2_uart.c           (PS/2 UART transport driver)
│       │   ├── ps2_uart.h           (internal header)
│       │   ├── trackpoint.c         (PS/2 protocol + input event emission)
│       │   └── trackpoint.h         (internal header)
│       ├── include/
│       │   └── zmk/
│       │       └── trackpoint.h     (public API if needed)
│       └── dts/
│           └── bindings/
│               ├── zmk,trackpoint-ps2.yaml       (main DT binding)
│               └── zmk,trackpoint-ps2-uart.yaml  (UART PS/2 transport binding)
└── (no ZMK fork needed)
```

## ZMK 0.3 Pointing System Reference

### What ZMK 0.3 provides (you don't implement these):

**Kconfig:** `CONFIG_ZMK_POINTING` (in `app/src/pointing/Kconfig`)
- Selects `INPUT` and `INPUT_THREAD_PRIORITY_OVERRIDE`
- Enables `ZMK_INPUT_LISTENER`, `ZMK_INPUT_SPLIT`, input processors

**HID API** (in `app/include/zmk/hid.h`, `app/src/hid.c`):
```c
// Report structure (16-bit X/Y, 16-bit scroll X/Y, 5 buttons)
struct zmk_hid_mouse_report_body {
    zmk_mouse_button_flags_t buttons;
    int16_t d_x;
    int16_t d_y;
    int16_t d_scroll_y;
    int16_t d_scroll_x;
} __packed;

// Functions (called by input_listener, NOT by your driver)
void zmk_hid_mouse_movement_set(int16_t x, int16_t y);
void zmk_hid_mouse_scroll_set(int16_t x, int16_t y);
int zmk_hid_mouse_button_press(zmk_mouse_button_t button);
int zmk_hid_mouse_button_release(zmk_mouse_button_t button);
void zmk_hid_mouse_clear(void);
```

**Endpoint sending** (in `app/src/endpoints.c`):
```c
int zmk_endpoint_send_mouse_report();  // dispatches to USB or BLE
```

**Input listener** (in `app/src/pointing/input_listener.c`):
- DTS compatible: `"zmk,input-listener"`
- Subscribes to Zephyr input events from a specified device
- Accumulates REL events, on sync calls HID functions and sends report
- Supports input processors and layer overrides

**Input processors** (in `app/src/pointing/`):
- `zmk,input-processor-scaler` — multiply/divide values
- `zmk,input-processor-transform` — swap axes, invert
- `zmk,input-processor-temp-layer` — temporary layer activation on input
- `zmk,input-processor-code- mapper` — remap event codes
- `zmk,input-processor-behaviors` — trigger behaviors from input

**Split transport** (in `app/src/pointing/input_split.c`):
- For split keyboards: peripheral forwards input events to central
- DTS compatible: `"zmk,input-split"`

### DTS binding for zmk,input-listener:
```yaml
compatible: "zmk,input-listener"
properties:
  device:        # phandle to the input device (your driver)
  input-processors:  # optional processing pipeline
```

### Example overlay wiring:
```dts
/ {
    my_trackpoint: trackpoint {
        compatible = "zmk,trackpoint-ps2";
        // ... properties ...
    };

    trackpoint_listener {
        compatible = "zmk,input-listener";
        device = <&my_trackpoint>;
    };
};
```

## Your Driver's Contract

Your driver must:

1. **Be a Zephyr input device** — register with `DEVICE_DT_INST_DEFINE()`
2. **Emit standard input events** using:
   ```c
   #include <zephyr/input/input.h>

   // Movement (relative)
   input_report(dev, INPUT_EV_REL, INPUT_REL_X, delta_x, false, K_NO_WAIT);
   input_report(dev, INPUT_EV_REL, INPUT_REL_Y, delta_y, false, K_NO_WAIT);

   // Buttons
   input_report(dev, INPUT_EV_KEY, INPUT_BTN_0, pressed, false, K_NO_WAIT);  // left
   input_report(dev, INPUT_EV_KEY, INPUT_BTN_1, pressed, false, K_NO_WAIT);  // right
   input_report(dev, INPUT_EV_KEY, INPUT_BTN_2, pressed, false, K_NO_WAIT);  // middle

   // Sync (signals end of a batch of events)
   input_report_sync(dev, K_NO_WAIT);
   ```
3. **NOT call ZMK HID functions** — the input listener handles that
4. **Configure TrackPoint hardware** via PS/2 commands (sensitivity, inertia, etc.)

## PS/2 UART Transport Layer Reference

Reference: `infused-kim/kb_zmk_ps2_mouse_trackpoint_driver` → `src/drivers/ps2/ps2_uart.c`

### Key concept: UART repurposed for PS/2

The nRF52840 has no PS/2 hardware. The trick is using the UART peripheral:
- **Reading:** UART RX pin mapped to PS/2 data (SDA) line. UART configured with even
  parity (nRF52 doesn't support odd parity). PS/2 uses odd parity, so every valid byte
  generates a parity error — this is used as a correctness check. If no parity error,
  the byte had even parity and is invalid.
- **Writing:** Switch pinctrl to `PINCTRL_STATE_SLEEP` (releases UART pins to GPIO),
  bit-bang the clock inhibit sequence, then use GPIO interrupt on SCL to clock out data
  bits with precise `k_busy_wait(69us)` timing.
- **Mode switching:** Read mode = `PINCTRL_STATE_DEFAULT` (UART controls SDA) + UART RX
  IRQ. Write mode = `PINCTRL_STATE_SLEEP` (GPIO controls both pins) + disable UART IRQ.

### PS/2 protocol basics

**Packet format (standard mouse):**
```
Byte 0: [Y overflow | X overflow | Y sign | X sign | 1 | Middle | Right | Left]
Byte 1: X movement (8 bits, sign in byte 0)
Byte 2: Y movement (8 bits, sign in byte 0)
Byte 3: Scroll (Intellimouse extension, if enabled)
```

**PS/2 commands your driver sends:**
```c
#define PS2_CMD_RESET           0xFF
#define PS2_CMD_ENABLE_REPORT   0xF4
#define PS2_CMD_DISABLE_REPORT  0xF5
#define PS2_CMD_GET_DEVICE_ID   0xF2
#define PS2_CMD_SET_SAMPLE_RATE 0xF3

// TrackPoint-specific (prefix with 0xE2):
#define TP_CMD_GET_SECONDARY_ID     {0xE1}           // response: 2 bytes
#define TP_CMD_SET_SENSITIVITY      {0xE2, 0x81, 0x4A, VALUE}
#define TP_CMD_SET_NEG_INERTIA      {0xE2, 0x81, 0x4D, VALUE}
#define TP_CMD_SET_VALUE6           {0xE2, 0x81, 0x60, VALUE}
#define TP_CMD_SET_PTS_THRESHOLD    {0xE2, 0x81, 0x5C, VALUE}
```

**Init sequence:**
1. Wait 600ms (TrackPoint power-on reset time)
2. Send `0xFF` (reset), wait for `0xAA` (self-test pass) + `0x00` (mouse ID)
3. Send `0xE1` to check if it's a trackpoint (get secondary ID)
4. Enable Intellimouse scroll mode: send rate sequence `200→100→80`, check for ID `0x03`
5. Configure TrackPoint parameters (sensitivity, inertia, etc.)
6. Send `0xF4` (enable reporting)
7. Register PS/2 callback, process incoming bytes into packets

### PS/2 UART write sequence:
1. Switch to write mode (pinctrl → SLEEP, disable UART IRQ, configure GPIOs as output)
2. Inhibit clock: set SCL low, SDA high, wait 500us
3. Set SDA low (start bit), wait 69us
4. Release SCL (configure as input), enable SCL falling-edge interrupt
5. On each SCL falling edge: set SDA to next data bit, wait 69us
6. After 8 data bits: set SDA to parity bit
7. After parity: set SDA high (stop bit), configure SDA as input
8. Read ACK from SDA (low = success)
9. Switch back to read mode (pinctrl → DEFAULT, enable UART IRQ)

### Timing constants:
```c
#define PS2_SCL_CYCLE_LEN       69   // microseconds per clock cycle
#define PS2_SCL_INHIBITION_MIN  100  // minimum inhibit time
#define PS2_SCL_INHIBITION      500  // actual inhibit time (5x min for reliability)
#define PS2_TIMEOUT_WRITE_RESP  K_MSEC(300)  // max wait for device response
```

## TrackPoint Protocol Reference

Reference: `infused-kim/kb_zmk_ps2_mouse_trackpoint_driver` → `src/drivers/input/input_mouse_ps2.c`

### TrackPoint configuration registers (via 0xE2 prefix):

| Parameter | Get Command | Set Command | Default | Range |
|-----------|-------------|-------------|---------|-------|
| Sensitivity | `E2 80 4A` | `E2 81 4A VAL` | 128 | 0-255 |
| Negative Inertia | `E2 80 4D` | `E2 81 4D VAL` | 6 | 0-255 |
| Upper Plateau Speed (val6) | `E2 80 60` | `E2 81 60 VAL` | 97 | 0-255 |
| Press-to-Select Threshold | `E2 80 5C` | `E2 81 5C VAL` | 8 | 0-255 |

### TrackPoint config byte bits (via `E2 80 2C` / `E2 81 2C`):
```c
#define TP_CONFIG_PRESS_TO_SELECT  BIT(0)
#define TP_CONFIG_INVERT_X         BIT(3)
#define TP_CONFIG_INVERT_Y         BIT(4)
#define TP_CONFIG_INVERT_Z         BIT(5)
#define TP_CONFIG_SWAP_XY          BIT(6)
```

### Intellimouse scroll mode activation:
```
Send: F3 200 → F3 100 → F3 80 → F2
Read: ID byte. If 0x03 or 0x04, scroll mode is active.
```

## DT Binding Design

### Main binding: `zmk,trackpoint-ps2.yaml`
```yaml
description: PS/2 TrackPoint pointing device

compatible: "zmk,trackpoint-ps2"

properties:
  ps2-device:
    type: phandle
    required: true
    description: Reference to PS/2 transport device (uart-ps2 or gpio-ps2)

  rst-gpios:
    type: phandle-array
    required: false
    description: Reset GPIO (active high)

  # TrackPoint parameters (compile-time defaults, runtime-adjustable)
  tp-sensitivity:
    type: int
    required: false
    default: 128
    description: TrackPoint sensitivity (0-255)

  tp-neg-inertia:
    type: int
    required: false
    default: 6
    description: TrackPoint negative inertia (0-255)

  tp-val6-upper-speed:
    type: int
    required: false
    default: 97
    description: TrackPoint upper plateau speed (0-255)

  tp-press-to-select:
    type: boolean
    required: false
    description: Enable press-to-select

  tp-press-to-select-threshold:
    type: int
    required: false
    default: 8
    description: Press-to-select threshold (0-255)

  # Axis configuration
  tp-x-invert:
    type: boolean
    required: false
  tp-y-invert:
    type: boolean
    required: false
  tp-xy-swap:
    type: boolean
    required: false

  # Scroll mode
  scroll-mode:
    type: boolean
    required: false
    description: Enable Intellimouse scroll extension

  sampling-rate:
    type: int
    required: false
    default: 100
    enum: [10, 20, 40, 60, 80, 100, 200]
```

### UART transport binding: `zmk,trackpoint-ps2-uart.yaml`
```yaml
description: UART-based PS/2 transport

compatible: "zmk,trackpoint-ps2-uart"

include: [uart-device.yaml]

child-bus: ps2

properties:
  scl-gpios:
    type: phandle-array
    required: true
    description: PS/2 clock line GPIO

  sda-gpios:
    type: phandle-array
    required: true
    description: PS/2 data line GPIO (also used as UART RX)
```

## Module Registration

### `modules/trackpoint/zephyr/module.yml`
```yaml
name: zmk-driver-trackpoint
build:
  cmake: ..
  kconfig: ../Kconfig
settings:
  dts_root: ..
```

### `modules/trackpoint/Kconfig`
```kconfig
menuconfig ZMK_TRACKPOINT
    bool "TrackPoint PS/2 Driver"
    depends on (!ZMK_SPLIT || ZMK_SPLIT_ROLE_CENTRAL)
    select ZMK_POINTING
    select PS2

if ZMK_TRACKPOINT

config ZMK_TRACKPOINT_PS2_UART
    bool "UART-based PS/2 transport"
    default y

endif
```

## flake.nix Changes

```nix
firmware = zmk-nix.legacyPackages.${system}.buildSplitKeyboard {
  name = "firmware";
  src = nixpkgs.lib.sourceFilesBySuffices self [
    ".board" ".cmake" ".conf" ".defconfig" ".dts" ".dtsi" ".json"
    ".keymap" ".overlay" ".shield" ".yml" "_defconfig"
    ".c" ".h" ".yaml"   # ← added for driver module source
  ];
  board = "nice_nano_v2";
  shield = "do52pro_%PART%";
  zephyrDepsHash = "sha256-...";  # update when west.yml changes

  # Point to local module
  extraCmakeFlags = [
    "-DZMK_EXTRA_MODULES=${self}/modules/trackpoint"
  ];
};
```

## config/west.yml Changes

Remove infused-kim. Use official ZMK:
```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
  self:
    path: config
```

## do52pro_right.overlay Changes

```dts
#include "do52pro.dtsi"

&default_transform {
    col-offset = <6>;
};

&kscan0 {
    col-gpios
        = <&pro_micro 6 GPIO_ACTIVE_HIGH>
        , <&pro_micro 5 GPIO_ACTIVE_HIGH>
        , <&pro_micro 4 GPIO_ACTIVE_HIGH>
        , <&pro_micro 2 GPIO_ACTIVE_HIGH>
        , <&pro_micro 0 GPIO_ACTIVE_HIGH>
        , <&pro_micro 1 GPIO_ACTIVE_HIGH>
        ;
};

// PS/2 TrackPoint via UART
&pinctrl {
    uart0_ps2_default: uart0_ps2_default {
        group1 {
            psels = <NRF_PSEL(UART_TX, 0, 27)>,
                    <NRF_PSEL(UART_RX, 0, 10)>;
        };
    };
    uart0_ps2_sleep: uart0_ps2_sleep {
        group1 {
            psels = <NRF_PSEL(UART_TX, 0, 27)>,
                    <NRF_PSEL(UART_RX, 0, 10)>;
            low-power-enable;
        };
    };
};

&uart0 {
    status = "okay";
    current-speed = <14400>;
    pinctrl-0 = <&uart0_ps2_default>;
    pinctrl-1 = <&uart0_ps2_sleep>;
    pinctrl-names = "default", "sleep";

    ps2_transport: trackpoint_ps2_uart {
        compatible = "zmk,trackpoint-ps2-uart";
        status = "okay";
        scl-gpios = <&pro_micro 15 GPIO_ACTIVE_HIGH>;
        sda-gpios = <&pro_micro 16 GPIO_ACTIVE_HIGH>;
    };
};

/ {
    trackpoint: trackpoint {
        compatible = "zmk,trackpoint-ps2";
        status = "okay";
        ps2-device = <&ps2_transport>;

        tp-sensitivity = <128>;
        tp-neg-inertia = <6>;
        tp-xy-swap;
    };

    trackpoint_listener: trackpoint_listener {
        compatible = "zmk,input-listener";
        device = <&trackpoint>;
    };
};
```

## Implementation Steps

1. **Create branch** `trackpoint-dev` in zmk-config repo
2. **Scaffold module structure** — `modules/trackpoint/` with module.yml, Kconfig, CMakeLists
3. **Implement PS/2 UART transport** (`ps2_uart.c`) — reference infused-kim's implementation
   but adapt to your DT binding. Key: UART RX for reading, GPIO bit-bang for writing,
   pinctrl mode switching.
4. **Implement TrackPoint protocol driver** (`trackpoint.c`) — PS/2 init sequence, packet
   parsing, TrackPoint config, emits `input_report()` events
5. **Create DT bindings** — YAML files for compatible strings
6. **Update overlay** — wire trackpoint to `zmk,input-listener`
7. **Update flake.nix** — expand source filter, add `ZMK_EXTRA_MODULES`
8. **Update west.yml** — point to official ZMK, remove infused-kim
9. **Build and test** — `nix build .#firmware`

## Key Differences from infused-kim's Implementation

| Aspect | infused-kim | Your ZMK 0.3 driver |
|--------|-------------|---------------------|
| Output mechanism | Calls `input_report()` then custom `input_listener_ps2.c` calls ZMK HID | Calls `input_report()` only; ZMK's native `zmk,input-listener` handles HID |
| Compatible strings | `"uart-ps2"`, `"zmk,input-mouse-ps2"`, `"zmk,input-listener-ps2"` | `"zmk,trackpoint-ps2-uart"`, `"zmk,trackpoint-ps2"` |
| Layer toggle | Custom `layer-toggle` property in listener + custom logic | Use `zmk,input-processor-temp-layer` |
| Axis transform | Custom `xy-swap`, `x-invert`, `y-invert` in listener | Use `zmk,input-processor-transform` |
| Scaling | Custom `scale-multiplier`/`scale-divisor` in listener | Use `zmk,input-processor-scaler` |
| Runtime tuning | Custom `&mms` behavior calling `zmk_mouse_ps2_tp_*_change()` | Implement as ZMK behavior or keep compile-time only |
| ZMK dependency | infused-kim ZMK fork (PR #2027) | Official ZMK main (0.3+) |
| Kconfig | `CONFIG_ZMK_MOUSE` | `CONFIG_ZMK_POINTING` |
| PS/2 API | Zephyr `ps2_driver_api` | Same (this is a Zephyr API, not ZMK) |

## Critical Implementation Details

### PS/2 UART parity trick (nRF52840):
```c
// Configure UART with even parity. PS/2 uses odd parity.
// Every valid PS/2 byte will generate a parity error.
// No parity error = byte had even parity = invalid.
uart_cfg.parity = UART_CFG_PARITY_EVEN;

// In the error handler:
int err = uart_err_check(dev);
if ((err & NRF_UARTE_ERROR_PARITY_MASK) == 0) {
    // No parity error → even parity → invalid PS/2 byte
    err = UART_ERROR_PARITY;
} else {
    // Parity error occurred → odd parity → valid PS/2 byte
    err = 0;
}
```

### Interrupt priority (critical for PS/2 timing):
PS/2 clock is 10-16.7kHz (60-100us per cycle). BLE controller interrupts must be
lower priority than PS/2 GPIO interrupts. In your overlay:
```dts
&gpiote { interrupts = < 6 0 >; };  // Highest priority for GPIO (PS/2 clock)
&radio { interrupts = < 1 3 >; };   // Lower priority for BLE
&uart0 { interrupts = < 2 3 >; };   // UART for PS/2 data
```

### Input event emission pattern:
```c
// In your PS/2 callback or work handler, after parsing a complete packet:
static void trackpoint_process_packet(const struct device *dev,
                                       struct trackpoint_data *data) {
    // Apply axis transforms from DT config
    int16_t x = data->current_packet.mov_x;
    int16_t y = data->current_packet.mov_y;

    if (data->config->tp_x_invert) x = -x;
    if (data->config->tp_y_invert) y = -y;
    if (data->config->tp_xy_swap) { int16_t tmp = x; x = y; y = tmp; }

    // Emit relative movement
    input_report(dev, INPUT_EV_REL, INPUT_REL_X, x, false, K_NO_WAIT);
    input_report(dev, INPUT_EV_REL, INPUT_REL_Y, y, true, K_NO_WAIT);

    // Emit scroll (if Intellimouse mode active)
    if (data->packet_mode == PACKET_MODE_SCROLL && data->current_packet.scroll) {
        input_report(dev, INPUT_EV_REL, INPUT_REL_WHEEL,
                     data->current_packet.scroll, false, K_NO_WAIT);
    }

    // Emit button state changes
    if (data->current_packet.button_l != data->prev_packet.button_l) {
        input_report(dev, INPUT_EV_KEY, INPUT_BTN_0,
                     data->current_packet.button_l, false, K_NO_WAIT);
    }
    if (data->current_packet.button_r != data->prev_packet.button_r) {
        input_report(dev, INPUT_EV_KEY, INPUT_BTN_1,
                     data->current_packet.button_r, false, K_NO_WAIT);
    }
    if (data->current_packet.button_m != data->prev_packet.button_m) {
        input_report(dev, INPUT_EV_KEY, INPUT_BTN_2,
                     data->current_packet.button_m, true, K_NO_WAIT);
    }

    // Final sync
    input_report_sync(dev, K_NO_WAIT);

    data->prev_packet = data->current_packet;
}
```

## Existing Project Context

**Board:** nice_nano_v2 (nRF52840)
**Shield:** do52pro (custom split keyboard)
**TrackPoint pins:** SCL=D15 (P1.13), SDA=D16 (P0.10)
**UART baud:** 14400 (matches ~14.9kHz trackpoint clock)
**Build system:** Nix flakes via `zmk-nix`
**Current config:** `config/do52pro.conf`, `config/do52pro.keymap`

Build commands:
```bash
nix build .#firmware          # build
nix run .#flash right         # flash right side (central)
nix run .#flash left          # flash left side
```

## Reference Sources (read-only, for understanding)

- **ZMK 0.3 pointing:** `zmkfirmware/zmk` → `app/src/pointing/` (input_listener.c,
  input_processor_temp_layer.c, input_processor_transform.c, input_processor_scaler.c)
- **ZMK 0.3 HID:** `zmkfirmware/zmk` → `app/src/hid.c`, `app/include/zmk/hid.h`
- **ZMK 0.3 endpoints:** `zmkfirmware/zmk` → `app/src/endpoints.c`
- **ZMK 0.3 Kconfig:** `zmkfirmware/zmk` → `app/src/pointing/Kconfig`
- **ZMK 0.3 DT bindings:** `zmkfirmware/zmk` → `app/dts/bindings/`
- **PS/2 UART reference:** `infused-kim/kb_zmk_ps2_mouse_trackpoint_driver` →
  `src/drivers/ps2/ps2_uart.c`
- **TrackPoint protocol reference:** `infused-kim/kb_zmk_ps2_mouse_trackpoint_driver` →
  `src/drivers/input/input_mouse_ps2.c`
- **Zephyr input API:** `zephyr/input/input.h` — `input_report()`, `input_report_sync()`
- **Zephyr PS/2 API:** `zephyr/drivers/ps2.h` — `ps2_driver_api` struct
