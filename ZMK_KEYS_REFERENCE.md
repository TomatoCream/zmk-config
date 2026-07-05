# ZMK Key Codes & Behaviors Reference

Complete reference for all key codes and behaviors available in ZMK keymaps.

## How Key Codes Are Encoded

Key codes are 32-bit values: `(usage_page << 16) | usage_id`. Modifier wrappers OR modifier bits into bits 24-31.

---

## Modifier Wrappers

Wrap any key code to add a modifier held simultaneously:

| Wrapper | Modifier |
|---------|----------|
| `LC(kc)` | Left Control |
| `LS(kc)` | Left Shift |
| `LA(kc)` | Left Alt |
| `LG(kc)` | Left GUI (Win/Cmd) |
| `RC(kc)` | Right Control |
| `RS(kc)` | Right Shift |
| `RA(kc)` | Right Alt (AltGr) |
| `RG(kc)` | Right GUI |

---

## Behavior References (`&` prefixed)

### Core Key Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&kp` | 1 | Key Press — press/release a keycode |
| `&kt` | 1 | Key Toggle — toggle a key on/off |
| `&sk` | 1 | Sticky Key — one-shot modifier (tap to apply to next keypress) |

### Layer Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&mo` | 1 | Momentary Layer — activate layer while held |
| `&lt` | 2 | Layer-Tap — hold=layer, tap=key `(layer, keycode)` |
| `&to` | 1 | To Layer — switch to layer exclusively |
| `&tog` | 1 | Toggle Layer — toggle layer on/off |
| `&sl` | 1 | Sticky Layer — one-shot layer (tap to activate for next keypress) |
| `&trans` | 0 | Transparent — pass through to lower layer |
| `&none` | 0 | None — no action (key does nothing) |

### Modifier / Combo Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&mt` | 2 | Mod-Tap — hold=modifier, tap=key `(modifier, keycode)` |
| `&gresc` | 0 | Grave/Escape — tap=Esc, tap with GUI+Shift=Grave |
| `&caps_word` | 0 | Caps Word — activates caps for next word |

### Bluetooth Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&bt` | 2 | Bluetooth control. `(command, param)` |

**BT commands:**

| Command | Value | Usage |
|---------|-------|-------|
| `BT_CLR` | 0 | Clear current profile |
| `BT_NXT` | 1 | Next profile |
| `BT_PRV` | 2 | Previous profile |
| `BT_SEL` | 3 | Select profile (e.g. `&bt BT_SEL 0`) |
| `BT_CLR_ALL` | 4 | Clear all profiles |
| `BT_DISC` | 5 | Start BLE advertising |

### Output Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&out` | 1 | Output Selection — switch between USB/BLE |

**Output values:**

| Value | Description |
|-------|-------------|
| `OUT_TOG` | Toggle output |
| `OUT_USB` | Select USB |
| `OUT_BLE` | Select BLE |

### Mouse Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&mkp` | 1 | Mouse Key Press — press/release mouse button. Use `MB1`..`MB5` |
| `&mmv` | 1 | Mouse Move — move mouse cursor. Use `MOVE_UP/DOWN/LEFT/RIGHT` |
| `&msc` | 1 | Mouse Scroll — scroll wheel. Use `SCRL_UP/DOWN/LEFT/RIGHT` |

**Mouse buttons (for `&mkp`):**

| Define | Alias | Description |
|--------|-------|-------------|
| `MB1` | `LCLK` | Left click |
| `MB2` | `RCLK` | Right click |
| `MB3` | `MCLK` | Middle click |
| `MB4` | — | Back |
| `MB5` | — | Forward |

**Mouse movement (for `&mmv`):**

| Define | Description |
|--------|-------------|
| `MOVE_UP` | Move up |
| `MOVE_DOWN` | Move down |
| `MOVE_LEFT` | Move left |
| `MOVE_RIGHT` | Move right |
| `MOVE(hor, vert)` | Custom move with explicit values |
| `MOVE_X(hor)` | Horizontal component only |
| `MOVE_Y(vert)` | Vertical component only |

**Mouse scroll (for `&msc`):**

| Define | Description |
|--------|-------------|
| `SCRL_UP` | Scroll up |
| `SCRL_DOWN` | Scroll down |
| `SCRL_LEFT` | Scroll left (horizontal) |
| `SCRL_RIGHT` | Scroll right (horizontal) |

### System / Reset Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&sys_reset` | 0 | System Reset — reboot the MCU |
| `&bootloader` | 0 | Bootloader — enter UF2 bootloader mode |
| `&soft_off` | 0 | Soft Off — put keyboard into soft-off state |

### LED / Lighting Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&rgb_ug` | 2 | RGB Underglow — control RGB LEDs `(command, value)` |
| `&bl` | 2 | Backlight — control backlight `(command, value)` |
| `&ext_power` | 1 | External Power — control external power rail |

**RGB commands:**

| Command | Value | Description |
|---------|-------|-------------|
| `RGB_TOG` | 0 | Toggle |
| `RGB_ON` | 1 | On |
| `RGB_OFF` | 2 | Off |
| `RGB_HUI` | 3 | Hue increase |
| `RGB_HUD` | 4 | Hue decrease |
| `RGB_SAI` | 5 | Saturation increase |
| `RGB_SAD` | 6 | Saturation decrease |
| `RGB_BRI` | 7 | Brightness increase |
| `RGB_BRD` | 8 | Brightness decrease |
| `RGB_SPI` | 9 | Speed increase |
| `RGB_SPD` | 10 | Speed decrease |
| `RGB_EFF` | 11 | Effect forward |
| `RGB_EFR` | 12 | Effect reverse |
| `RGB_EFS` | 13 | Effect select |
| `RGB_COLOR_HSB` | 14 | Set HSB color |

**Backlight commands:**

| Command | Value | Description |
|---------|-------|-------------|
| `BL_ON` | 0 | On |
| `BL_OFF` | 1 | Off |
| `BL_TOG` | 2 | Toggle |
| `BL_INC` | 3 | Brightness increase |
| `BL_DEC` | 4 | Brightness decrease |
| `BL_CYCLE` | 5 | Cycle |
| `BL_SET` | 6 | Set level |

**External power commands:**

| Command | Value | Description |
|---------|-------|-------------|
| `EP_ON` | 1 | On |
| `EP_OFF` | 0 | Off |
| `EP_TOG` | 2 | Toggle |

### Sensor / Encoder Behaviors

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&inc_dec_kp` | 2 | Encoder Key Press — CW=key1, CCW=key2 for rotary encoders |

### Macro Infrastructure

| Reference | Cells | Description |
|-----------|-------|-------------|
| `&macro_tap` | 0 | Macro control: tap mode |
| `&macro_press` | 0 | Macro control: press mode |
| `&macro_release` | 0 | Macro control: release mode |
| `&macro_tap_time` | 1 | Macro control: set tap duration (ms) |
| `&macro_wait_time` | 1 | Macro control: set wait time (ms) |
| `&macro_pause_for_release` | 0 | Macro control: pause until key release |
| `&macro_param_1to1` | 0 | Macro param mapping: 1-to-1 |
| `&macro_param_1to2` | 0 | Macro param mapping: 1-to-2 |
| `&macro_param_2to1` | 0 | Macro param mapping: 2-to-1 |
| `&macro_param_2to2` | 0 | Macro param mapping: 2-to-2 |

---

## System Keys (HID Page 0x01: Generic Desktop)

| Define | Alias | Description |
|--------|-------|-------------|
| `SYSTEM_POWER` | `SYS_PWR` | System Power Down |
| `SYSTEM_SLEEP` | `SYS_SLEEP` | System Sleep |
| `SYSTEM_WAKE_UP` | `SYS_WAKE` | System Wake Up |

---

## Letters (HID Page 0x07)

| Define | Alias | HID ID |
|--------|-------|--------|
| `A` | — | `0x04` |
| `B` | — | `0x05` |
| `C` | — | `0x06` |
| `D` | — | `0x07` |
| `E` | — | `0x08` |
| `F` | — | `0x09` |
| `G` | — | `0x0A` |
| `H` | — | `0x0B` |
| `I` | — | `0x0C` |
| `J` | — | `0x0D` |
| `K` | — | `0x0E` |
| `L` | — | `0x0F` |
| `M` | — | `0x10` |
| `N` | — | `0x11` |
| `O` | — | `0x12` |
| `P` | — | `0x13` |
| `Q` | — | `0x14` |
| `R` | — | `0x15` |
| `S` | — | `0x16` |
| `T` | — | `0x17` |
| `U` | — | `0x18` |
| `V` | — | `0x19` |
| `W` | — | `0x1A` |
| `X` | — | `0x1B` |
| `Y` | — | `0x1C` |
| `Z` | — | `0x1D` |

---

## Numbers (Top Row)

| Define | Alias | HID ID | Shifted Symbol |
|--------|-------|--------|----------------|
| `NUMBER_1` | `N1` | `0x1E` | `!` → use `EXCLAMATION` |
| `NUMBER_2` | `N2` | `0x1F` | `@` → use `AT_SIGN` |
| `NUMBER_3` | `N3` | `0x20` | `#` → use `HASH` |
| `NUMBER_4` | `N4` | `0x21` | `$` → use `DOLLAR` |
| `NUMBER_5` | `N5` | `0x22` | `%` → use `PERCENT` |
| `NUMBER_6` | `N6` | `0x23` | `^` → use `CARET` |
| `NUMBER_7` | `N7` | `0x24` | `&` → use `AMPERSAND` |
| `NUMBER_8` | `N8` | `0x25` | `*` → use `ASTERISK` |
| `NUMBER_9` | `N9` | `0x26` | `(` → use `LEFT_PARENTHESIS` |
| `NUMBER_0` | `N0` | `0x27` | `)` → use `RIGHT_PARENTHESIS` |

---

## Pre-shifted Symbol Defines

| Define | Alias | Equivalent |
|--------|-------|------------|
| `EXCLAMATION` | `EXCL` | `LS(N1)` — `!` |
| `AT_SIGN` | `AT`, `ATSN` | `LS(N2)` — `@` |
| `HASH` | `POUND` | `LS(N3)` — `#` |
| `DOLLAR` | `DLLR` | `LS(N4)` — `$` |
| `PERCENT` | `PRCNT`, `PRCT` | `LS(N5)` — `%` |
| `CARET` | `CRRT` | `LS(N6)` — `^` |
| `AMPERSAND` | `AMPS` | `LS(N7)` — `&` |
| `ASTERISK` | `ASTRK`, `STAR` | `LS(N8)` — `*` |
| `LEFT_PARENTHESIS` | `LPAR`, `LPRN` | `LS(N9)` — `(` |
| `RIGHT_PARENTHESIS` | `RPAR`, `RPRN` | `LS(N0)` — `)` |

---

## Whitespace / Editing Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `RETURN` | `ENTER`, `RET` | `0x28` |
| `ESCAPE` | `ESC` | `0x29` |
| `BACKSPACE` | `BSPC`, `BKSP` | `0x2A` |
| `TAB` | — | `0x2B` |
| `SPACE` | `SPC` | `0x2C` |

---

## Punctuation / Symbol Keys

| Define | Alias | HID ID | Shifted |
|--------|-------|--------|---------|
| `MINUS` | — | `0x2D` | `_` → use `UNDERSCORE` |
| `UNDERSCORE` | `UNDER` | `LS(0x2D)` | `_` |
| `EQUAL` | `EQL` | `0x2E` | `+` → use `PLUS` |
| `PLUS` | — | `LS(0x2E)` | `+` |
| `LEFT_BRACKET` | `LBKT` | `0x2F` | `{` → use `LEFT_BRACE` |
| `LEFT_BRACE` | `LBRC` | `LS(0x2F)` | `{` |
| `RIGHT_BRACKET` | `RBKT` | `0x30` | `}` → use `RIGHT_BRACE` |
| `RIGHT_BRACE` | `RBRC` | `LS(0x30)` | `}` |
| `BACKSLASH` | `BSLH` | `0x31` | `\|` → use `PIPE` |
| `PIPE` | — | `LS(0x31)` | `\|` |
| `NON_US_HASH` | `NUHS` | `0x32` | `~` → use `TILDE2` |
| `TILDE2` | — | `LS(0x32)` | `~` (non-US) |
| `SEMICOLON` | `SEMI`, `SCLN` | `0x33` | `:` → use `COLON` |
| `COLON` | `COLN` | `LS(0x33)` | `:` |
| `SINGLE_QUOTE` | `SQT`, `APOSTROPHE`, `APOS`, `QUOT` | `0x34` | `"` → use `DOUBLE_QUOTES` |
| `DOUBLE_QUOTES` | `DQT` | `LS(0x34)` | `"` |
| `GRAVE` | `GRAV` | `0x35` | `~` → use `TILDE` |
| `TILDE` | `TILD` | `LS(0x35)` | `~` |
| `COMMA` | `CMMA` | `0x36` | `<` → use `LESS_THAN` |
| `LESS_THAN` | `LT`, `LABT` | `LS(0x36)` | `<` |
| `PERIOD` | `DOT` | `0x37` | `>` → use `GREATER_THAN` |
| `GREATER_THAN` | `GT`, `RABT` | `LS(0x37)` | `>` |
| `SLASH` | `FSLH` | `0x38` | `?` → use `QUESTION` |
| `QUESTION` | `QMARK` | `LS(0x38)` | `?` |

---

## Lock / Special Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `CAPSLOCK` | `CAPS`, `CLCK` | `0x39` |
| `PRINTSCREEN` | `PSCRN`, `PRSC` | `0x46` |
| `SCROLLLOCK` | `SLCK`, `SCLK` | `0x47` |
| `PAUSE_BREAK` | `PAUS` | `0x48` |

---

## Navigation Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `INSERT` | `INS` | `0x49` |
| `HOME` | — | `0x4A` |
| `PAGE_UP` | `PG_UP`, `PGUP` | `0x4B` |
| `DELETE` | `DEL` | `0x4C` |
| `END` | — | `0x4D` |
| `PAGE_DOWN` | `PG_DN`, `PGDN` | `0x4E` |
| `RIGHT_ARROW` | `RIGHT`, `RARW` | `0x4F` |
| `LEFT_ARROW` | `LEFT`, `LARW` | `0x50` |
| `DOWN_ARROW` | `DOWN`, `DARW` | `0x51` |
| `UP_ARROW` | `UP`, `UARW` | `0x52` |

---

## Function Keys

| Define | HID ID | Define | HID ID |
|--------|--------|--------|--------|
| `F1` | `0x3A` | `F13` | `0x68` |
| `F2` | `0x3B` | `F14` | `0x69` |
| `F3` | `0x3C` | `F15` | `0x6A` |
| `F4` | `0x3D` | `F16` | `0x6B` |
| `F5` | `0x3E` | `F17` | `0x6C` |
| `F6` | `0x3F` | `F18` | `0x6D` |
| `F7` | `0x40` | `F19` | `0x6E` |
| `F8` | `0x41` | `F20` | `0x6F` |
| `F9` | `0x42` | `F21` | `0x70` |
| `F10` | `0x43` | `F22` | `0x71` |
| `F11` | `0x44` | `F23` | `0x72` |
| `F12` | `0x45` | `F24` | `0x73` |

---

## Keypad Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `KP_NUMLOCK` | `KP_NUM`, `KP_NLCK` | `0x53` |
| `KP_DIVIDE` | `KP_SLASH`, `KDIV` | `0x54` |
| `KP_MULTIPLY` | `KP_ASTERISK`, `KMLT` | `0x55` |
| `KP_MINUS` | `KP_SUBTRACT`, `KMIN` | `0x56` |
| `KP_PLUS` | `KPLS` | `0x57` |
| `KP_ENTER` | — | `0x58` |
| `KP_NUMBER_1` | `KP_N1` | `0x59` |
| `KP_NUMBER_2` | `KP_N2` | `0x5A` |
| `KP_NUMBER_3` | `KP_N3` | `0x5B` |
| `KP_NUMBER_4` | `KP_N4` | `0x5C` |
| `KP_NUMBER_5` | `KP_N5` | `0x5D` |
| `KP_NUMBER_6` | `KP_N6` | `0x5E` |
| `KP_NUMBER_7` | `KP_N7` | `0x5F` |
| `KP_NUMBER_8` | `KP_N8` | `0x60` |
| `KP_NUMBER_9` | `KP_N9` | `0x61` |
| `KP_NUMBER_0` | `KP_N0` | `0x62` |
| `KP_DOT` | — | `0x63` |
| `KP_EQUAL` | — | `0x67` |
| `KP_COMMA` | — | `0x85` |
| `KP_EQUAL_AS400` | — | `0x86` |
| `KP_LEFT_PARENTHESIS` | `KP_LPAR` | `0xB6` |
| `KP_RIGHT_PARENTHESIS` | `KP_RPAR` | `0xB7` |
| `KP_CLEAR` | — | `0xD8` |

---

## Non-US Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `NON_US_BACKSLASH` | `NON_US_BSLH`, `NUBS` | `0x64` |

---

## Modifier Keys (Standalone)

| Define | Alias | HID ID |
|--------|-------|--------|
| `LEFT_CONTROL` | `LCTRL`, `LCTL` | `0xE0` |
| `LEFT_SHIFT` | `LSHIFT`, `LSHFT`, `LSFT` | `0xE1` |
| `LEFT_ALT` | `LALT` | `0xE2` |
| `LEFT_GUI` | `LGUI`, `LEFT_WIN`, `LWIN`, `LEFT_COMMAND`, `LCMD`, `LEFT_META`, `LMETA` | `0xE3` |
| `RIGHT_CONTROL` | `RCTRL`, `RCTL` | `0xE4` |
| `RIGHT_SHIFT` | `RSHIFT`, `RSHFT`, `RSFT` | `0xE5` |
| `RIGHT_ALT` | `RALT` | `0xE6` |
| `RIGHT_GUI` | `RGUI`, `RIGHT_WIN`, `RWIN`, `RIGHT_COMMAND`, `RCMD`, `RIGHT_META`, `RMETA` | `0xE7` |

---

## Miscellaneous Keyboard Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `K_APPLICATION` | `K_APP`, `K_CONTEXT_MENU`, `K_CMENU`, `GUI` | `0x65` |
| `K_POWER` | `K_PWR` | `0x66` |
| `K_EXECUTE` | `K_EXEC` | `0x74` |
| `K_HELP` | — | `0x75` |
| `K_MENU` | — | `0x76` |
| `K_SELECT` | — | `0x77` |
| `K_STOP` | — | `0x78` |
| `K_AGAIN` | `K_REDO` | `0x79` |
| `K_UNDO` | `UNDO` | `0x7A` |
| `K_CUT` | `CUT` | `0x7B` |
| `K_COPY` | `COPY` | `0x7C` |
| `K_PASTE` | `PSTE` | `0x7D` |
| `K_FIND` | — | `0x7E` |
| `K_MUTE` | — | `0x7F` |
| `K_VOLUME_UP` | `K_VOL_UP`, `VOLU` | `0x80` |
| `K_VOLUME_DOWN` | `K_VOL_DN`, `VOLD` | `0x81` |
| `LOCKING_CAPS` | `LCAPS` | `0x82` |
| `LOCKING_NUM` | `LNLCK` | `0x83` |
| `LOCKING_SCROLL` | `LSLCK` | `0x84` |

---

## International / Language Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `INTERNATIONAL_1` | `INT1`, `INT_RO` | `0x87` |
| `INTERNATIONAL_2` | `INT2`, `INT_KATAKANAHIRAGANA`, `INT_KANA` | `0x88` |
| `INTERNATIONAL_3` | `INT3`, `INT_YEN` | `0x89` |
| `INTERNATIONAL_4` | `INT4`, `INT_HENKAN` | `0x8A` |
| `INTERNATIONAL_5` | `INT5`, `INT_MUHENKAN` | `0x8B` |
| `INTERNATIONAL_6` | `INT6`, `INT_KPJPCOMMA` | `0x8C` |
| `INTERNATIONAL_7` | `INT7` | `0x8D` |
| `INTERNATIONAL_8` | `INT8` | `0x8E` |
| `INTERNATIONAL_9` | `INT9` | `0x8F` |
| `LANGUAGE_1` | `LANG1`, `LANG_HANGEUL` | `0x90` |
| `LANGUAGE_2` | `LANG2`, `LANG_HANJA` | `0x91` |
| `LANGUAGE_3` | `LANG3`, `LANG_KATAKANA` | `0x92` |
| `LANGUAGE_4` | `LANG4`, `LANG_HIRAGANA` | `0x93` |
| `LANGUAGE_5` | `LANG5`, `LANG_ZENKAKUHANKAKU` | `0x94` |
| `LANGUAGE_6` | `LANG6` | `0x95` |
| `LANGUAGE_7` | `LANG7` | `0x96` |
| `LANGUAGE_8` | `LANG8` | `0x97` |
| `LANGUAGE_9` | `LANG9` | `0x98` |

---

## Extended Keyboard Keys

| Define | Alias | HID ID |
|--------|-------|--------|
| `ALT_ERASE` | — | `0x99` |
| `SYSREQ` | `ATTENTION` | `0x9A` |
| `K_CANCEL` | — | `0x9B` |
| `CLEAR` | — | `0x9C` |
| `PRIOR` | — | `0x9D` |
| `RETURN2` | `RET2` | `0x9E` |
| `SEPARATOR` | — | `0x9F` |
| `OUT` | — | `0xA0` |
| `OPER` | — | `0xA1` |
| `CLEAR_AGAIN` | — | `0xA2` |
| `CRSEL` | — | `0xA3` |
| `EXSEL` | — | `0xA4` |

---

## Media / Consumer Keys (Keyboard Page, 0xE8-0xFB)

| Define | Alias | HID ID |
|--------|-------|--------|
| `K_PLAY_PAUSE` | `K_PP` | `0xE8` |
| `K_STOP2` | — | `0xE9` |
| `K_PREVIOUS` | `K_PREV` | `0xEA` |
| `K_NEXT` | — | `0xEB` |
| `K_EJECT` | — | `0xEC` |
| `K_VOLUME_UP2` | `K_VOL_UP2` | `0xED` |
| `K_VOLUME_DOWN2` | `K_VOL_DN2` | `0xEE` |
| `K_MUTE2` | — | `0xEF` |
| `K_WWW` | — | `0xF0` |
| `K_BACK` | — | `0xF1` |
| `K_FORWARD` | — | `0xF2` |
| `K_STOP3` | — | `0xF3` |
| `K_FIND2` | — | `0xF4` |
| `K_SCROLL_UP` | — | `0xF5` |
| `K_SCROLL_DOWN` | — | `0xF6` |
| `K_EDIT` | — | `0xF7` |
| `K_SLEEP` | — | `0xF8` |
| `K_LOCK` | `K_SCREENSAVER`, `K_COFFEE` | `0xF9` |
| `K_REFRESH` | — | `0xFA` |
| `K_CALCULATOR` | `K_CALC` | `0xFB` |

---

## Consumer Page Keys (HID Page 0x0C)

### Power / System

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_POWER` | `C_PWR` | `0x30` |
| `C_RESET` | — | `0x31` |
| `C_SLEEP` | — | `0x32` |
| `C_SLEEP_MODE` | — | `0x34` |
| `C_MENU` | — | `0x40` |
| `C_MENU_PICK` | `C_MENU_SELECT` | `0x41` |
| `C_MENU_UP` | — | `0x42` |
| `C_MENU_DOWN` | — | `0x43` |
| `C_MENU_LEFT` | — | `0x44` |
| `C_MENU_RIGHT` | — | `0x45` |
| `C_MENU_ESCAPE` | `C_MENU_ESC` | `0x46` |
| `C_MENU_INCREASE` | `C_MENU_INC` | `0x47` |
| `C_MENU_DECREASE` | `C_MENU_DEC` | `0x48` |

### Display / Brightness

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_DATA_ON_SCREEN` | — | `0x60` |
| `C_CAPTIONS` | `C_SUBTITLES` | `0x61` |
| `C_SNAPSHOT` | — | `0x65` |
| `C_PIP` | — | `0x67` |
| `C_RED_BUTTON` | `C_RED` | `0x69` |
| `C_GREEN_BUTTON` | `C_GREEN` | `0x6A` |
| `C_BLUE_BUTTON` | `C_BLUE` | `0x6B` |
| `C_YELLOW_BUTTON` | `C_YELLOW` | `0x6C` |
| `C_ASPECT` | — | `0x6D` |
| `C_BRIGHTNESS_INC` | `C_BRI_INC`, `C_BRI_UP` | `0x6F` |
| `C_BRIGHTNESS_DEC` | `C_BRI_DEC`, `C_BRI_DN` | `0x70` |
| `C_BACKLIGHT_TOGGLE` | `C_BKLT_TOG` | `0x72` |
| `C_BRIGHTNESS_MINIMUM` | `C_BRI_MIN` | `0x73` |
| `C_BRIGHTNESS_MAXIMUM` | `C_BRI_MAX` | `0x74` |
| `C_BRIGHTNESS_AUTO` | `C_BRI_AUTO` | `0x75` |

### Channel / Media Selection

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_MEDIA_STEP` | `C_MODE_STEP` | `0x82` |
| `C_RECALL_LAST` | `C_CHAN_LAST` | `0x83` |
| `C_MEDIA_COMPUTER` | — | `0x88` |
| `C_MEDIA_TV` | — | `0x89` |
| `C_MEDIA_WWW` | — | `0x8A` |
| `C_MEDIA_DVD` | — | `0x8B` |
| `C_MEDIA_PHONE` | — | `0x8C` |
| `C_MEDIA_GUIDE` | — | `0x8D` |
| `C_MEDIA_VIDEOPHONE` | — | `0x8E` |
| `C_MEDIA_GAMES` | — | `0x8F` |
| `C_MEDIA_MESSAGES` | — | `0x90` |
| `C_MEDIA_CD` | — | `0x91` |
| `C_MEDIA_VCR` | — | `0x92` |
| `C_MEDIA_TUNER` | — | `0x93` |
| `C_QUIT` | — | `0x94` |
| `C_HELP` | — | `0x95` |
| `C_MEDIA_TAPE` | — | `0x96` |
| `C_MEDIA_CABLE` | — | `0x97` |
| `C_MEDIA_SATELLITE` | — | `0x98` |
| `C_MEDIA_HOME` | — | `0x9A` |
| `C_CHANNEL_INC` | `C_CHAN_INC` | `0x9C` |
| `C_CHANNEL_DEC` | `C_CHAN_DEC` | `0x9D` |
| `C_MEDIA_VCR_PLUS` | — | `0xA0` |

### Transport Controls

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_PLAY` | — | `0xB0` |
| `C_PAUSE` | — | `0xB1` |
| `C_RECORD` | `C_REC` | `0xB2` |
| `C_FAST_FORWARD` | `C_FF` | `0xB3` |
| `C_REWIND` | `C_RW` | `0xB4` |
| `C_NEXT` | `M_NEXT` | `0xB5` |
| `C_PREVIOUS` | `C_PREV`, `M_PREV` | `0xB6` |
| `C_STOP` | `M_STOP` | `0xB7` |
| `C_EJECT` | `M_EJCT` | `0xB8` |
| `C_RANDOM_PLAY` | `C_SHUFFLE` | `0xB9` |
| `C_REPEAT` | — | `0xBC` |
| `C_SLOW_TRACKING` | `C_SLOW2` | `0xBF` |
| `C_STOP_EJECT` | — | `0xCC` |
| `C_PLAY_PAUSE` | `C_PP`, `M_PLAY` | `0xCD` |
| `C_VOICE_COMMAND` | — | `0xCF` |

### Volume / Audio

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_MUTE` | `M_MUTE` | `0xE2` |
| `C_BASS_BOOST` | — | `0xE5` |
| `C_VOLUME_UP` | `C_VOL_UP`, `M_VOLU` | `0xE9` |
| `C_VOLUME_DOWN` | `C_VOL_DN`, `M_VOLD` | `0xEA` |
| `C_SLOW` | — | `0xF5` |
| `C_ALTERNATE_AUDIO_INCREMENT` | `C_ALT_AUDIO_INC` | `0x173` |

### Application Launch Keys (AL)

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_AL_CCC` | — | `0x183` |
| `C_AL_WORD` | — | `0x184` |
| `C_AL_TEXT_EDITOR` | — | `0x185` |
| `C_AL_SPREADSHEET` | `C_AL_SHEET` | `0x186` |
| `C_AL_GRAPHICS_EDITOR` | — | `0x187` |
| `C_AL_PRESENTATION` | — | `0x188` |
| `C_AL_DATABASE` | `C_AL_DB` | `0x189` |
| `C_AL_EMAIL` | `C_AL_MAIL` | `0x18A` |
| `C_AL_NEWS` | — | `0x18B` |
| `C_AL_VOICEMAIL` | — | `0x18C` |
| `C_AL_CONTACTS` | `C_AL_ADDRESS_BOOK` | `0x18D` |
| `C_AL_CALENDAR` | `C_AL_CAL` | `0x18E` |
| `C_AL_TASK_MANAGER` | — | `0x18F` |
| `C_AL_JOURNAL` | — | `0x190` |
| `C_AL_FINANCE` | — | `0x191` |
| `C_AL_CALCULATOR` | `C_AL_CALC` | `0x192` |
| `C_AL_AV_CAPTURE_PLAYBACK` | — | `0x193` |
| `C_AL_MY_COMPUTER` | — | `0x194` |
| `C_AL_WWW` | — | `0x196` |
| `C_AL_NETWORK_CHAT` | `C_AL_CHAT` | `0x199` |
| `C_AL_LOGOFF` | — | `0x19C` |
| `C_AL_LOCK` | `C_AL_SCREENSAVER`, `C_AL_COFFEE` | `0x19E` |
| `C_AL_CONTROL_PANEL` | — | `0x19F` |
| `C_AL_SELECT_TASK` | — | `0x1A2` |
| `C_AL_NEXT_TASK` | — | `0x1A3` |
| `C_AL_PREVIOUS_TASK` | `C_AL_PREV_TASK` | `0x1A4` |
| `C_AL_HELP` | — | `0x1A6` |
| `C_AL_DOCUMENTS` | `C_AL_DOCS` | `0x1A7` |
| `C_AL_SPELLCHECK` | `C_AL_SPELL` | `0x1AB` |
| `C_AL_KEYBOARD_LAYOUT` | — | `0x1AE` |
| `C_AL_SCREEN_SAVER` | — | `0x1B1` |
| `C_AL_FILE_BROWSER` | `C_AL_FILES` | `0x1B4` |
| `C_AL_IMAGE_BROWSER` | `C_AL_IMAGES` | `0x1B6` |
| `C_AL_AUDIO_BROWSER` | `C_AL_AUDIO`, `C_AL_MUSIC` | `0x1B7` |
| `C_AL_MOVIE_BROWSER` | `C_AL_MOVIES` | `0x1B8` |
| `C_AL_INSTANT_MESSAGING` | `C_AL_IM` | `0x1BC` |
| `C_AL_OEM_FEATURES` | `C_AL_TIPS`, `C_AL_TUTORIAL` | `0x1BD` |

### GUI Application Control Keys (AC)

| Define | Alias | HID ID |
|--------|-------|--------|
| `C_AC_NEW` | — | `0x201` |
| `C_AC_OPEN` | — | `0x202` |
| `C_AC_CLOSE` | — | `0x203` |
| `C_AC_EXIT` | — | `0x204` |
| `C_AC_SAVE` | — | `0x207` |
| `C_AC_PRINT` | — | `0x208` |
| `C_AC_PROPERTIES` | `C_AC_PROPS` | `0x209` |
| `C_AC_UNDO` | — | `0x21A` |
| `C_AC_COPY` | — | `0x21B` |
| `C_AC_CUT` | — | `0x21C` |
| `C_AC_PASTE` | — | `0x21D` |
| `C_AC_FIND` | — | `0x21F` |
| `C_AC_SEARCH` | — | `0x221` |
| `C_AC_GOTO` | — | `0x222` |
| `C_AC_HOME` | — | `0x223` |
| `C_AC_BACK` | — | `0x224` |
| `C_AC_FORWARD` | — | `0x225` |
| `C_AC_STOP` | — | `0x226` |
| `C_AC_REFRESH` | — | `0x227` |
| `C_AC_BOOKMARKS` | `C_AC_FAVORITES`, `C_AC_FAVOURITES` | `0x22A` |
| `C_AC_ZOOM_IN` | — | `0x22D` |
| `C_AC_ZOOM_OUT` | — | `0x22E` |
| `C_AC_ZOOM` | — | `0x22F` |
| `C_AC_VIEW_TOGGLE` | — | `0x232` |
| `C_AC_SCROLL_UP` | — | `0x233` |
| `C_AC_SCROLL_DOWN` | — | `0x234` |
| `C_AC_EDIT` | — | `0x23D` |
| `C_AC_CANCEL` | — | `0x25F` |
| `C_AC_INSERT` | `C_AC_INS` | `0x269` |
| `C_AC_DEL` | — | `0x26A` |
| `C_AC_REDO` | — | `0x279` |
| `C_AC_REPLY` | — | `0x289` |
| `C_AC_FORWARD_MAIL` | — | `0x28B` |
| `C_AC_SEND` | — | `0x28C` |
| `C_AC_DESKTOP_SHOW_ALL_WINDOWS` | — | `0x29F` |
| `C_AC_DESKTOP_SHOW_ALL_APPLICATIONS` | — | `0x2A2` |
| `C_KEYBOARD_INPUT_ASSIST_PREVIOUS` | `C_KBIA_PREV` | `0x2C7` |
| `C_KEYBOARD_INPUT_ASSIST_NEXT` | `C_KBIA_NEXT` | `0x2C8` |
| `C_KEYBOARD_INPUT_ASSIST_PREVIOUS_GROUP` | `C_KBIA_PREV_GRP` | `0x2C9` |
| `C_KEYBOARD_INPUT_ASSIST_NEXT_GROUP` | `C_KBIA_NEXT_GRP` | `0x2CA` |
| `C_KEYBOARD_INPUT_ASSIST_ACCEPT` | `C_KBIA_ACCEPT` | `0x2CB` |
| `C_KEYBOARD_INPUT_ASSIST_CANCEL` | `C_KBIA_CANCEL` | `0x2CC` |
| `C_AC_NEXT_KEYBOARD_LAYOUT_SELECT` | `GLOBE` | `0x29D` |

---

## Available Includes

These headers can be included in keymap files:

```c
#include <dt-bindings/zmk/keys.h>          // Main key codes (auto-includes hid_usage, hid_usage_pages, modifiers)
#include <dt-bindings/zmk/mouse.h>          // Mouse buttons, movement, scroll
#include <dt-bindings/zmk/bt.h>             // Bluetooth commands
#include <dt-bindings/zmk/outputs.h>        // Output selection
#include <dt-bindings/zmk/reset.h>          // Reset/bootloader
#include <dt-bindings/zmk/rgb.h>            // RGB underglow
#include <dt-bindings/zmk/backlight.h>      // Backlight
#include <dt-bindings/zmk/ext_power.h>      // External power
#include <dt-bindings/zmk/hid_usage.h>      // Raw HID usage IDs
#include <dt-bindings/zmk/hid_usage_pages.h> // HID usage page numbers
#include <dt-bindings/zmk/modifiers.h>      // Modifier bit masks
```

---

*Generated from ZMK v0.3 source at `/nix/store/bbswnc9j22j6v4s046mxsybsdx9csg4s-firmware-west-deps/zmk/`*
