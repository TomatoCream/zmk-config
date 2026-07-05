# Copyright (c) 2026 The ZMK Contributors
# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_TRACKPOINT)
  zephyr_library_named(zmk_trackpoint)
  zephyr_library_sources(src/trackpoint.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_TRACKPOINT_PS2_UART src/ps2_uart.c)
endif()
