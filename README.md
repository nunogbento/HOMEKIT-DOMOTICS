# HOMEKIT-DOMOTICS

Personal collection of home-automation hardware and firmware around Apple HomeKit,
spanning multiple generations of boards and HomeKit integration techniques.

## Project layout

Each top-level folder is one project. Projects with a custom PCB have:

```
<Project>/
├── firmware/      one subfolder per firmware version (generation)
├── circuit/       PCB design files (.brd / .sch / .cam)
└── enclosure/     3D-printable bodies (where applicable)
```

Projects without a custom PCB omit `circuit/`, and use `enclosure/` only if a
3D-printed body exists.

## Active projects

### [PCCWDplus](PCCWDplus/) — custom PCB
Main panel board. Succession: MQTT (ESP8266) → HomeSpan (ESP32-C5).
Gen 2 (HAP/ESP8266) was attempted but failed; see archive notes.
- `firmware/Mqtt.MordomusESP8266/` — gen 1, MQTT/ESP8266 — **deployed**
- `firmware/HSPCCWDplus/` — gen 3, HomeSpan/ESP32-C5 — **WIP**, pin-compatible drop-in

### [MultiAccessory](MultiAccessory/) — custom PCB
Multi-device controller board. Gen 2 still deployed, gen 3 in progress.
- `firmware/HKMultiAccessory/` — gen 2, HAP/ESP8266 — **deployed**
- `firmware/HSPMultiAccessory/` — gen 3, HomeSpan/ESP32-C5 — **WIP**
- `firmware/HKSprinklerValve/` — gen 2 valve variant on the same board

### [ESP8266-ADC](ESP8266-ADC/) — custom PCB
Shared ESP8266-with-ADC board used for analog sensing applications.
- `firmware/HKSolarPanel/` — gen 2, HAP/ESP8266 — **deployed**, no successor planned
- `firmware/mqtt-SolarPanel/` — gen 1, archived firmware
- `firmware/Mqtt.ESP8266PlantComputer/` — gen 1, archived firmware

### [HSPCCWD](HSPCCWD/) — no custom hardware
HomeSpan reference implementation on a bare ESP32 devkit, serial-only.

### [ESP32Door](ESP32Door/) — no custom hardware
Door sensor using an off-the-shelf ESP32 module + 3D-printed enclosure.

### [Feeder](Feeder/) — no custom hardware, 3D-printed body
- `firmware/HKFEEDER/` — current
- `firmware/mqtt-RabitFeeder/` — earlier version

## HomeKit firmware generations

| Gen | Stack           | Chip         | Status                                |
|-----|-----------------|--------------|---------------------------------------|
| 1   | MQTT bridge     | ESP8266      | Mqtt.MordomusESP8266 still deployed   |
| 2   | Arduino-HomeKit | ESP8266      | HKMultiAccessory, HKSolarPanel still deployed; HKPCCWD archived (failed) |
| 3   | HomeSpan        | ESP32 / C5   | HSPCCWDplus, HSPMultiAccessory — WIP  |

Gen 2 failed on the PCCWD board because the ESP8266 could not handle HomeKit
at that board's accessory complexity. That's why the PCCWD lineage jumps
directly from gen 1 (MQTT) to gen 3 (HomeSpan).

## archive/

Everything superseded, retired, unfinished, unrelated, or not tied to an
active hardware project. Includes:
- `HKPCCWD/` (gen 2 failure)
- Early MQTT experiments (lightbulbs, rollerblinds, power meter, etc.)
- Side experiments (CameraWebServer, ESP-32CAM, SONOFFAPI, WaterDetector)
- `Tradfri-ICC-1/` — reverse-engineering research that informed MultiAccessory
- `MordomusAPI/`, `PowerMeter/`, `pccwdapi/`, `node-red-contrib-pccwd/` — no longer used
- `homebridge-host/` — Homebridge configuration, Node-RED flows, backups
