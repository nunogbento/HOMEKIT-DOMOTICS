# ESP32-C6 Zigbee Solar-Thermal Monitor

Replacement for the ESP8266 + ADS1115 board in [`ESP8266-ADC`](../ESP8266-ADC) that has monitored the
solar hot-water system since 2026-06. Same two clip-on CTs, same tank probe, plus **two strap-on pipe
probes** for hot-water draw detection, moved to the **ESP32-C6 + Zigbee** platform proven by the
[wall input module](../ESP32C6_ZIGBEE_Wall_Input_Module) (router, OTA with rollback, crash post-mortem
over the mesh, external Z2M converter).

Why (personal-ops `reports/2026-09-19-solar-review-and-plan.md`): the solar loop has an intermittent
fault that the tank probe alone reveals only in hindsight, and the smart electric-backup algorithm
(T-0035) needs real hot-water demand data. The tank probe cannot see a morning shower; the pipe pair can.

## What it measures

| EP | Cluster | Meaning |
|---|---|---|
| 10 | msTemperatureMeasurement | DS18B20 probe A (lowest ROM) — role set in Z2M (`probe_roles`), default **tank**; OTA client lives here |
| 11 | msTemperatureMeasurement | probe B — default **hot_out** (DHW outlet pipe, strap-on under insulation) |
| 12 | msTemperatureMeasurement | probe C — default **cold_in** (mains inlet pipe, strap-on) |
| 13 | msTemperatureMeasurement | C6 die temperature (diagnostic) |
| 14 | genAnalogInput | electric heater current, A RMS (CT ch0-1) |
| 15 | genAnalogInput | solar circulator current, A RMS (CT ch2-3) |
| 16 | genAnalogInput/Output | crash post-mortem in the two `description` attributes; write 13579 to the output = crash test |

Values refresh every 60 s. **Nothing is reported by the device; Z2M polls** (`onEvent` timer in the
converter). Do not bind or configureReporting any endpoint — every report path faults this zboss build
(see the wall module's v4–v10 history).

Not measured: **collector temperature.** The Bosch TDS 100-2 controller has no data bus and drives an
NTC 20K collector probe whose ground reference is unverified — see HARDWARE.md §Collector before
connecting anything to it.

## Commissioning
1. Flash `build/…merged.bin` at 0x0 over J3 (blank board), power-cycle, `permit_join` in Z2M, wait for the
   interview (model `ESP32C6-SOLAR-MON`). Converter: `diy_esp32c6_solar_monitor.js` → Pi
   `~/zigbee2mqtt/data/external_converters/`.
2. Identify probes: warm each DS18B20 in your hand, watch which of the three temperatures moves, then set
   the device option `probe_roles` (e.g. `hot_out,tank,cold_in`) so EP10/11/12 get the right names.
3. Clip the CTs: ch0-1 on the **heater** phase, ch2-3 on the **circulator** phase (single conductor, not the
   whole cable). `heater_running` (> 1 A) and `circulator_running` (> 0.1 A) mirror the old firmware.
4. HA: the existing `packages/solar_monitoring.yaml` templates key on the old MQTT entities — repoint them
   to the new Z2M entities (`sensor.<name>_tank_temperature`, `_heater_current`, `_circulator_current`) and
   the Node-RED `SolarPanelTelemetry` archiver to the new topic. Keep field names `tanktemperature`,
   `currentRMSE`, `currentRMSS` so the InfluxDB history stays continuous.

## Firmware updates
Same OTA flow as the wall module (`ota/make_ota.py`, Z2M `ota_override`), with its own image type
**0x1012** so a wall image can never land on this board. Bench-test on the old ESP8266's successor board
before OTA'ing the installed unit — rollback saves a crash-looping image, not a wrong one.
