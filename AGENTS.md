# AGENTS.md — zmk-config for do52pro

## Project Overview

ZMK firmware config for a **do52pro** split keyboard with:
- **Board:** nice_nano_v2 (nRF52840)
- **Shield:** do52pro (custom, defined in `boards/shields/do52pro/`)
- **Build system:** Nix flakes via `zmk-nix`
- **PS/2 TrackPoint:** UART driver on right side (SCL=D15, SDA=D16)

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
│   ├── do52pro_right.overlay          # Right column GPIOs + PS/2 trackpoint
│   ├── do52pro.zmk.yml                # Shield metadata
│   ├── do52pro.conf                   # Shield-level Kconfig defaults
│   ├── Kconfig.defconfig              # Central role (right side)
│   └── Kconfig.shield                 # Shield enable logic
├── config/
│   ├── west.yml                       # West manifest (ZMK fork + modules)
│   ├── do52pro.keymap                 # 8-layer keymap
│   ├── do52pro.conf                   # Kconfig options
│   └── include/
│       └── mouse_tp.dtsi              # Trackpoint behaviors + layer toggle
└── zephyr/
    └── module.yml                     # Makes repo a Zephyr module (for custom shield)
```

## Key Configuration Files

### flake.nix
- `board` — ZMK board identifier
- `shield` — Shield name with `%PART%` placeholder for split
- `zephyrDepsHash` — Fixed-output hash for west deps. **Must change** when `west.yml` changes. Set to `""` to get the correct hash from build error.

### config/west.yml
- Uses `infused-kim/zmk` at `pr-testing/mouse_ps2_module_base` (mouse pointer PR)
- Trackpoint module: `infused-kim/kb_zmk_ps2_mouse_trackpoint_driver`
- Changing the ZMK fork or module requires updating `zephyrDepsHash`

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
| 5 | mouse_keys | Software mouse (auto-TP touch) |
| 6 | mouse_tp | Trackpoint scroll + clicks |
| 7 | mouse_settings | Runtime TP tuning (`&mms N`) |

### &mms behavior params
1=sens+, 2=sens-, 3=val6+, 4=val6-, 5=neg_inertia+, 6=neg_inertia-, 7=pts+, 8=pts-, 9=reset, 10=log

## TrackPoint Configuration

Defined in `boards/shields/do52pro/do52pro_right.overlay`:
- **Driver:** UART (PS/2 over UART hardware)
- **SCL:** D15 → pro_micro 15 (P1.13)
- **SDA:** D16 → pro_micro 16 (P0.10)
- **Baud:** 14400 (matches ~14.9kHz trackpoint clock)
- **Axes:** swapped (`xy-swap`) for 90deg rotation
- **Layer toggle:** auto-activates layer 5 on touch

## Gotchas

- **Nix flakes only see git-tracked files.** Run `git add` before building.
- **`.gitignore` blocks `/zephyr/`** except `zephyr/module.yml` (needed for custom shield).
- **Changing `west.yml`** requires updating `zephyrDepsHash`. Set to `""`, build, get hash from error, update.
- **Left side** has no PS/2 — dummy mouse layers keep layer count consistent.
- **Right must be flashed first** (it's the BLE central).
