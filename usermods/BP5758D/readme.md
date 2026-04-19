# BP5758D usermod

This usermod lets WLED output one RGB+CCT color to a BP5758D 5-channel LED driver using a bit-banged two-wire protocol.
It is intended to be build-target agnostic (ESP8266/ESP32 supported by WLED toolchains).

## Hardware defaults

- Data: `GPIO5` (D1 on many NodeMCU/ESP-12E boards)
- Clock: `GPIO4` (D2 on many NodeMCU/ESP-12E boards)

If your board wiring is different, override these with build flags:

```ini
build_flags =
  ${common.build_flags}
  -D BP5758D_DATA_PIN=5
  -D BP5758D_CLK_PIN=4
```

## Enable in WLED build

In your `platformio_override.ini`:

```ini
[env:nodemcuv2]
extends = env:d1_mini
board = nodemcuv2
custom_usermods =
  BP5758D
```

Then build and flash:

```bash
python -m platformio run -e nodemcuv2 -t upload
```

For ESP32 targets, use your chosen ESP32 environment instead, for example:

```bash
python -m platformio run -e esp32dev -t upload
```

## WLED runtime setup

In WLED LED settings, configure the main bus as a single RGBW pixel (count `1`).
This usermod reads pixel `0` and forwards it to BP5758D channels:

- Red -> channel R
- Green -> channel G
- Blue -> channel B
- White -> split into CW/WW using segment CCT

The CCT slider controls split:

- `0` = all warm white
- `255` = all cold white

## Notes

- This usermod initializes channel current limits on boot.
- Keep pin assignment in `build_flags` when moving between board families.
