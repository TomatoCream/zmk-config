# LEARNINGS.md

## Environment & Config

- Build: `nix build .#firmware` (NixOS host). Nix flakes only see
  git-tracked files — `git add` before building.
- `zephyrDepsHash` in flake.nix must be recomputed whenever
  `config/west.yml` changes: set to `""`, build, copy the `got:` hash
  from the error output.
- zmk-nix (`nix/zmk/keyboard.nix`) auto-passes the repo root as
  `-DZMK_EXTRA_MODULES` when `zephyr/module.yml` exists. Do NOT add another
  `-DZMK_EXTRA_MODULES` via `extraCmakeFlags` — it would be overridden.
  Register in-repo driver modules through `zephyr/module.yml`
  (build.cmake / build.kconfig / settings.dts_root) instead.
- Inspect a nix build log for a store path: `nix log /nix/store/...-firmware-right`.
  Useful greps: `Building C object .*zmk_trackpoint`, `pointing/.*\.c`.

## Codebase Conventions

- The PS/2 TrackPoint driver lives in `modules/trackpoint/` and is a plain
  Zephyr input device (emits `input_report_*`); ZMK's native
  `zmk,input-listener` (v0.3) does HID. Never call ZMK APIs from the driver.
- Driver Kconfig symbols gate on devicetree compatibles
  (`$(dt_compat_enabled,...)`), so the left half automatically builds
  without the driver.
- Interrupt priority overrides in `do52pro_right.overlay` (GPIOTE prio 0,
  radio 1, everything else 3) are required for PS/2 timing vs BLE — keep
  them when touching the overlay.

## Hardware Gotchas

- **After flashing, the trackpoint may appear dead until the keyboard is
  power-cycled (unplug/replug USB).** A firmware reset doesn't cut power to
  the TrackPoint, so it never re-sends its power-on self-test byte; the
  driver gives up after 10 init attempts (~30s). Always power-cycle before
  concluding the driver is broken.

## Business Context

- ZMK 0.3 (`zmkfirmware/zmk` tag v0.3) ships the pointing subsystem
  natively: `CONFIG_ZMK_POINTING`, `zmk,input-listener`, input processors
  (`zip_xy_transform`, `zip_temp_layer`, ...), `&mmv/&mkp/&msc` behaviors,
  `dt-bindings/zmk/pointing.h`. The infused-kim fork is no longer needed;
  only the PS/2 driver itself is missing upstream.
- Runtime TP tuning (`&mms`) was an infused-kim extra; the native driver
  uses compile-time DT properties (keymap layer 7 is currently unused).
