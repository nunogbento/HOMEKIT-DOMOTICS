# ESP32-C6 Zigbee MultiAccessory

Zigbee firmware for the MultiAccessory board, replacing the WiFi/HomeKit
(HomeSpan) firmware. Mains/12 V powered **WT0132C6-S5** — the board joins as a
**Zigbee router**, and HomeKit now comes via Home Assistant's bridge rather than
directly. Tracked as **T-0036** in `personal-ops`.

The board: **4 PWM MOSFET outputs**, a transistor-driven **IR LED** (LG splits),
and **I2C** for an AM2320 temperature/humidity sensor.

## One image for the whole fleet

The old firmware selected its configuration with compile-time `#define`s, so
every board needed its own build. Zigbee fixes endpoint composition at interview
time, but it can be **declared at boot from NVS** — so here the **output profile**
is a Multistate Output on EP14, writable from the Z2M UI and persisted. Writing it
reboots the board; **Z2M must then re-interview the device** for the new endpoints
to appear.

The **AM2320 is auto-detected**, so "does this board have a sensor" is not a
configuration axis at all: EP20 simply doesn't exist when no sensor answers.

| Profile | CH1 (IO6) | CH2 (IO5) | CH3 (IO4) | CH4 (IO2) | Status |
|---|---|---|---|---|---|
| `4XDIM` | dimmer | dimmer | dimmer | dimmer | ✅ v1 |
| `CCT_2DIM` | CCT cool | CCT warm | dimmer | dimmer | phase 2 |
| `2XCCT` | CCT cool | CCT warm | CCT cool | CCT warm | deferred |
| `RGBW` | B | G | R | W | deferred |
| `RGB_DIM` | B | G | R | dimmer | deferred |

An unimplemented profile falls back to `4XDIM` with a log line — a config value
can never brick a board.

## Endpoints

| EP | What | Notes |
|---|---|---|
| 10-13 | Dimmable Light, one per channel | OTA client lives on EP10 |
| 14 | Multistate Output | output profile selector |
| 15 | Analog Input | brownout / unexpected-reset counter |
| 16 | Temperature | C6 die temperature (diagnostic) |
| 20 | Temperature + Humidity | AM2320 — **only if detected** |
| 30 | *reserved* | the composed AC endpoint (phase 3) |

## ⚠️ No attribute reporting, anywhere

Every Zigbee report path faults this ESP32-C6 zboss build — learned the hard way
on the wall-input module: the app's `report*()` asserts in
`esp_zigbee_zcl_command.c:263`, and `setReporting()` / the coordinator's
`ConfigureReporting` null-deref the stack's own send path
(`zb_zcl_send_report_attr_command`). Therefore:

- the firmware only ever **sets** attribute values, and never calls
  `set*Reporting()` or `report*()`;
- the converter **polls** with reads (the read-response path works) and never
  binds+configures reporting — note `configureReporting` already defaults to
  `false` in Z2M's `light()`;
- lights don't need reporting anyway: Z2M is optimistic after a command.

## Pin map

Same PCB pads the ESP8266 build used — the module swap is proven (both live C6
boards were ESP8266 first, no rework). Only the GPIO numbers behind the pads
differ:

| | ESP8266 | WT0132C6-S5 |
|---|---|---|
| channels | 13 / 14 / 16 / 12 | **6 / 5 / 4 / 2** |
| IR LED | 10 | **21** |
| I2C | SDA 4 / SCL 5 | **SDA 10 / SCL 3** |
| status LED / control | — | **8 / 9** (9 is also IO9/BOOT) |

## Build & deploy

```
arduino-cli compile --fqbn "esp32:esp32:esp32c6:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr" .
```
v1 is 788 KB, 60% of the ZCZR app partition. **OTA ships in v1 on purpose:**
flashing Zigbee removes the WiFi OTA path, and most of these boards sit behind
furniture. OTA image type is **0x1012**, distinct from the wall-input module's
0x1011, so the two DIY devices can never see each other's images in the Z2M
override index. Images are rollback-guarded — a crash-looping OTA reverts.

Converter: copy `diy_esp32c6_multiaccessory.js` to
`~/zigbee2mqtt/data/external_converters/` and restart Z2M.

## Removing the old ESP8266 module

Cutting beats desoldering: nibble through the module's own PCB just inboard of
the castellations with a rotary tool, then clear the individual stubs with an iron
and braid. Proven on board six — dead module (expected), 100% intact pads, a few
minutes' work, and no bulk heat anywhere near the HLK-PM01 or the electrolytics.
