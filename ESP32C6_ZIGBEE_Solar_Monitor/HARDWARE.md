# Hardware — ESP32-C6 Solar-Thermal Monitor

## Modules
- **WT0132C6-S5** (ESP32-C6, ESP-12F land pattern) — same module and mains front-end (HLK-PM01 → 3V3 LDO)
  as the wall input module; reuse that power section verbatim.
- **ADS1115** 16-bit ADC, I²C address 0x48 (ADDR → GND). Gain 1 (±4.096 V), 860 SPS.
- **3× DS18B20** on one bus, 4k7 pull-up to 3V3. Tank probe = the existing immersion probe; the two pipe
  probes are TO-92 or cable-type sensors strapped to bare copper with a cable tie + thermal paste, under
  the pipe insulation, within ~30 cm of the tank connections.
- **2× SCT-013-000 style CT** 100 A / 50 mA, each into a **33 Ω burden** with a **VDD/2 bias** (2×10k
  divider + 10 µF) on the ADS1115 negative input, exactly the ESP8266-ADC front-end (`FACTOR 60.6` A/V).

## Pins (proposal — fix when the PCB is drawn)
| Signal | GPIO | Note |
|---|---|---|
| OneWire (DS18B20 ×3) | IO4 | 4k7 pull-up |
| I²C SDA | IO5 | ADS1115 |
| I²C SCL | IO6 | ADS1115 |
| Status LED (active low) | IO7 | as on the wall module (module pin 10) |
| BOOT / factory reset | IO9 | hold 5 s = leave network + rejoin |
| UART0 TX/RX | IO16/IO17 | J3 flashing header, GND/TX/RX (+3V3 power option), never with mains on |

## Safety
- The board is mains powered. CTs are clip-on and galvanically isolated from the conductor; the DS18B20s
  touch pipes at earth potential. Nothing on this board connects to the controller.
- Never flash with mains on J1 (adapter ground would be mains-referenced).

## Collector temperature — read this before touching the TDS 100-2
The Bosch/Junkers TDS 100-2 has **no VBus or any data port** (terminals L N R1 N, S1..S3 only) and uses
**NTC 20K** (collector, T1) / **NTC 10K** (tank) sensors — not Pt1000, not Resol-made. Its supply is a
1 W class II unit and may be transformerless, so the sensor common can sit at mains potential.
1. **Earth test first:** controller live, measure AC and DC volts from each T1 terminal to a real earth.
   Near zero → SELV, a direct tap is acceptable. Anything else → only an isolated read, or option 3.
2. **Tap (if SELV):** two rail-to-rail op-amp followers (e.g. MCP6002) with 100 kΩ series protection,
   into a **second ADS1115 at 0x49** differential pair. Scope the excitation with the PicoScope first
   (continuous DC vs pulsed). Calibrate voltage → temperature against the controller's display.
3. **Own sensor:** a thin NTC/Pt1000 rated ≥ 180 °C alongside T1 in the collector well, own excitation
   resistor, same second ADS1115. Independent of the controller; a DS18B20 dies at 125 °C there.
A strap-on DS18B20 on the solar FLOW pipe near the tank is a cheap partial substitute: it tracks the
collector under flow and diverges during stagnation — and needs only a fourth probe on the existing bus.
