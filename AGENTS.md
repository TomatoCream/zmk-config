# AGENTS.md — zmk-config for do52pro

## Project Overview

ZMK firmware config for a **do52pro** split keyboard with:
- **Board:** nice_nano_v2 (nRF52840)
- **Shield:** do52pro (custom, defined in `boards/shields/do52pro/`)
- **Build system:** Nix flakes via `zmk-nix`
- **ZMK:** official `zmkfirmware/zmk` at `v0.3` (native pointing subsystem)
- **PS/2 TrackPoint:** in-repo Zephyr input driver (`modules/trackpoint/`), right side (SCL=D15, SDA=D16)

## Quick Reference

```bash
# Build firmware
nix build .#firmware

# Flash right side first (central), then left
nix run .#flash right
nix run .#flash left
nix run .#flash            # both sequentially

# Update west dependencies
nix run .#update
```

## Directory Structure

```
.
├── flake.nix                          # Nix build config (board, shield, hash)
├── flake.lock                         # Pinned Nix inputs
├── boards/shields/do52pro/
│   ├── do52pro.dtsi                   # Matrix transform, kscan, physical layout
│   ├── do52pro_left.overlay           # Left column GPIOs
│   ├── do52pro_right.overlay          # Right column GPIOs + PS/2 trackpoint + input listener
│   ├── do52pro.zmk.yml                # Shield metadata
│   ├── do52pro.conf                   # Shield-level Kconfig defaults
│   ├── Kconfig.defconfig              # Central role (right side)
│   └── Kconfig.shield                 # Shield enable logic
├── config/
│   ├── west.yml                       # West manifest (official zmkfirmware/zmk v0.3)
│   ├── do52pro.keymap                 # 8-layer keymap (native &mmv/&mkp/&msc)
│   └── do52pro.conf                   # Kconfig options (CONFIG_ZMK_POINTING=y)
├── modules/trackpoint/                # In-repo PS/2 TrackPoint driver module
│   ├── CMakeLists.txt
│   ├── Kconfig                        # ZMK_TRACKPOINT, auto-enabled by DT compat
│   ├── src/ps2_uart.c                 # PS/2-over-UART transport (ps2_driver_api)
│   ├── src/trackpoint.c               # TrackPoint protocol -> Zephyr input events
│   └── dts/bindings/                  # zmk,trackpoint-ps2(-uart) bindings
└── zephyr/
    └── module.yml                     # Registers repo as Zephyr module
                                       # (board_root + trackpoint cmake/kconfig/dts_root)
```

## TrackPoint Driver Architecture (ZMK 0.3 native)

```
modules/trackpoint (Zephyr input device)
    │  emits input_report_rel/key (INPUT_REL_X/Y/WHEEL, INPUT_BTN_0/1/2)
    ▼
zmk,input-listener (ZMK core, wired in do52pro_right.overlay)
    │  input-processors: zip_xy_transform (XY swap), zip_temp_layer 6 250
    ▼
ZMK HID -> BLE HOG / USB
```

- The driver calls **no ZMK APIs** — only Zephyr's input subsystem, so it
  works with unmodified upstream ZMK.
- `zmk-nix` passes the repo root as `ZMK_EXTRA_MODULES` (because
  `zephyr/module.yml` exists), so the driver module is registered via that
  file — no extra flake plumbing.
- Driver Kconfig auto-enables from the DT compatibles
  (`zmk,trackpoint-ps2`, `zmk,trackpoint-ps2-uart`), i.e. right side only.
- Runtime tuning (`&mms`) from the old infused-kim setup is gone; tune via
  DT properties on the `trackpoint` node (`tp-sensitivity`,
  `tp-neg-inertia`, `tp-val6-upper-speed`, `scroll-mode`, ...).

## Key Configuration Files

### flake.nix
- `board` / `shield` — ZMK board and shield (`do52pro_%PART%`)
- `zephyrDepsHash` — Fixed-output hash for west deps. **Must change** when
  `west.yml` changes. Set to `""`, build, copy the `got:` hash from the error.
- `src` suffix filter includes `.c .h .yaml CMakeLists.txt Kconfig` for the
  driver module.

### boards/shields/do52pro/Kconfig.defconfig
- `SHIELD_DO52PRO_RIGHT` sets `ZMK_SPLIT_ROLE_CENTRAL=y` (right is central)
- Central side connects to host BLE and runs the PS/2 driver

## Keymap Layers

| Layer | Name | Purpose |
|-------|------|---------|
| 0 | qwerty | Base QWERTY |
| 1 | lower_layer | Numbers + navigation |
| 2 | upper_layer | Symbols |
| 3 | func_layer | F-keys |
| 4 | config_layer | Bluetooth, power |
| 5 | mouse_keys | Software mouse (&mmv/&mkp/&msc) |
| 6 | mouse_tp | Auto-activated by TP movement (zip_temp_layer): scroll + clicks |
| 7 | mouse_settings | Unused (runtime TP tuning not supported on native driver) |

## TrackPoint Configuration

Defined in `boards/shields/do52pro/do52pro_right.overlay`:
- **Transport:** PS/2 over UART (`zmk,trackpoint-ps2-uart` child of `&uart0`)
- **SCL:** D15 → pro_micro 15 (P1.13)
- **SDA:** D16 → pro_micro 16 (P0.10, also UART RX in pinctrl default state)
- **Baud:** 14400 (matches ~14.9kHz trackpoint clock)
- **Axes:** swapped via `zip_xy_transform (INPUT_TRANSFORM_XY_SWAP)`
- **Layer toggle:** `zip_temp_layer 6 250` activates layer 6 on movement
- Interrupt priority overrides in the overlay keep GPIOTE at priority 0
  (PS/2 clock) above the BLE radio — do not remove them.

## Gotchas

- **Nix flakes only see git-tracked files.** Run `git add` before building.
- **`.gitignore` blocks `/modules/*`** except `/modules/trackpoint` (west
  checkouts land in `/modules` in local west workspaces).
- **`.gitignore` blocks `/zephyr/`** except `zephyr/module.yml` (needed for
  custom shield + driver module registration).
- **Changing `west.yml`** requires updating `zephyrDepsHash`. Set to `""`,
  build, get hash from error, update.
- **Right must be flashed first** (it's the BLE central).
- Left side builds without the driver (Kconfig gates on DT compat, which
  only the right overlay defines); mouse layers use native behaviors so no
  dummy layers are needed.
