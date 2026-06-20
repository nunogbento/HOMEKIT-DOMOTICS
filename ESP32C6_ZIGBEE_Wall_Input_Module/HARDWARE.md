# ESP32-C6 Zigbee In-Wall Input Module — Circuit Design

> **KiCad project:** [`circuit/ESP32C6_Wall_Input_Module.kicad_pro`](circuit/) —
> full schematic (KiCad 10, ERC-clean), with footprints assigned to every part
> and a plotted PDF. The WT0132C6-S5 symbol is reused from the PCCWDplus
> project (bundled project-locally in `circuit/WT0132.kicad_sym`); the land
> pattern is the stock `RF_Module:ESP-12E`.

Mains-powered alternative to the Philips Hue Wall Switch Module. The lamp
circuit is bridged permanently live (smart bulbs never lose power), the two
existing wall switches are repurposed as low-voltage inputs, and the module
acts as a **Zigbee router** strengthening the mesh.

> ⚠️ **SAFETY** — This device connects directly to 230 VAC mains. Primary side
> must be fused, varistor-protected, and physically separated from the
> low-voltage side (≥ 6 mm creepage, milled slot under the Hi-Link if you make
> a PCB). The wall-switch wires become SELV signal wires — they must be fully
> disconnected from mains before connecting to SW1/SW2. Installation in a wall
> box requires a **Neutral** present (unlike the battery-powered Hue module).
> If your switch box has no Neutral, install the module at the ceiling
> rose/luminaire instead and run the switch wires to it.

## Block diagram

```
 MAINS L ──┬── F1 ──┬──[HLK-PM01]── +5V ──[AMS1117-3.3]── +3V3 ──[WT0132C6-S5]
           │        │ AC    DC                                        │
           │       RV1                                          GPIO4/GPIO5
 MAINS N ──┼────────┴──[HLK-PM01]                                     │
           │                                                   wall switches
 LAMP  L ──┘  (bridged with a Wago in the box:                  (dry contact
 LAMP  N ── N  lamp permanently live, off-board)                 to GND)
```

## Schematic (net-level)

### 1. Mains input & AC-DC (primary side)

```
J1-2 (L in) ──── F1 (fuse 500 mA T, 250 V) ───┬─── HLK-PM01 "AC L"
                                              │
                                             RV1 (MOV 275 VAC, S07K275)
                                              │
J1-1 (N in) ──────────────────────────────────┴─── HLK-PM01 "AC N"

Lamp feed: NOT on the board. Join lamp L to permanent L with a Wago
lever connector in the box — lamp always live, no load current on the PCB.
```

- **J1**: 2-position screw terminal, 5.08 mm pitch (Phoenix MKDS 1,5/2),
  L/N in only. Dropping the lamp pass-through keeps the board small and the
  lamp current off the copper entirely.
- **F1**: 500 mA time-lag (T) 250 V **TR5 radial micro fuse** (Littelfuse
  392/395 or LCSC equivalent) — a fraction of a 5×20 holder's space.
- **RV1**: metal-oxide varistor 275 VAC, 7 mm disc (S07K275).

### 2. 5 V rail (HLK-PM01 secondary)

```
HLK-PM01 "+Vo" ──┬──────┬────────── 5V
                 │      │
                C1     C2
              100 µF  100 nF
               16 V
                 │      │
HLK-PM01 "-Vo" ──┴──────┴────────── GND
```

- **PSU**: Hi-Link **HLK-PM01** (230 VAC → 5 V / 600 mA, 3 W). Plenty:
  ESP32-C6 Zigbee TX peaks are ~150 mA on the 3V3 rail.

### 3. 3.3 V rail (AMS1117-3.3)

```
5V ──┬───────── VIN [AMS1117-3.3] VOUT ─────┬──────┬───── 3V3
     │                  GND                 │      │
    C3                   │                 C4     C5
   10 µF                 │                22 µF  100 nF
     │                   │                 │      │
GND ─┴───────────────────┴─────────────────┴──────┴───── GND
```

- **C3**: 10 µF input cap. **C4**: 22 µF tantalum or low-ESR ceramic on the
  output — the AMS1117 needs this for stability. **C5**: 100 nF close to the
  regulator.
- Dissipation: (5 − 3.3 V) × 0.15 A ≈ 0.26 W peak — SOT-223 with a small
  copper pour is fine.

### 4. WT0132C6-S5 core (ESP32-C6, ESP-12F land pattern)

The WT0132C6-S5 puts an ESP32-C6 in the classic ESP8266 ESP-12F form factor:
side castellations only, easily hand-soldered, and a drop-in upgrade for older
ESP-12 boards. It exposes exactly what this design needs.

```
3V3 ──┬──────────────── pin 8  (VCC)
      │
     C6 47 µF + C7 100 nF  (right at the module pins)
      │
GND ──┴──────────────── pin 9  (GND)

3V3 ── R1 10k ──┬────── pin 3  (EN)
                │
               C8 1 µF
                │
               GND
SW3 (BOOT, SMD tactile) ── IO9 (pin 18, bottom row) ── button to GND
(no RESET button: pulse EN from the programming header or power-cycle)

3V3 ── LED1 ── R2 1k ── IO18 (pin 15)   (status LED, active LOW)
```

- Inputs stay on **IO4 (pin 5)** and **IO5 (pin 6)** — same GPIOs as the
  sketch has always used.
- **GPIO15 is not exposed** on this module, so the status LED moves to
  **IO18** (no strapping role). The sketch's `BOARD_WT0132C6_S5` block
  already sets `STATUS_LED 18`.
- UART0 (IO16/IO17) and IO9 sit on the **bottom castellation row** — still
  edge-solderable, unlike the WROOM's underside thermal pad.

Keep the antenna end of the module hanging over the board edge or over
a copper keep-out — no ground plane, no mains tracks under the antenna.

### 5. Switch inputs (the whole point)

Two identical channels. Wall switch is a dry contact between the input
terminal and GND. External pull-up + RC filter, because in-wall runs are long
and often share conduit with mains.

```
                 3V3
                  │
                 R3 10k                 (channel 2: R4/R5… identical,
                  │                      J2-2 → GPIO5)
J2-1 (SW1) ───────┼──── R5 1k ──── GPIO4
                  │
                 C9 100 nF
                  │
J2-3 (COM) ───── GND
```

- **J2**: 3-position screw terminal, 5.08 mm pitch (MKDS 1,5/3): `SW1 | SW2 | COM`.
- **R3/R4**: 10 kΩ pull-ups to 3V3 (the internal pull-up is enabled too —
  combined ~7 kΩ keeps the line stiff against induced noise).
- **R5/R6**: 1 kΩ series resistors — current limit / ESD protection for the
  GPIO.
- **C9/C10**: 100 nF to GND at the terminal side of the series resistor —
  hardware debounce + mains-coupled noise filter (≈1 ms time constant with
  the pull-up).
- Logic: switch closed → GPIO LOW → Zigbee `contact: false`.
- Works with **rocker/toggle switches** (state = position) and **momentary
  push buttons** (state pulses) — automations trigger on any change.

### 6. Programming header

```
J3 (1.27 mm, 6 pins): 3V3 | GND | TX (GPIO16) | RX (GPIO17) | EN | IO9 (BOOT)
```

Flash with a USB-UART adapter: hold IO9 low, pulse EN, upload.
(The native USB pins GPIO12/13 are not exposed on the WT0132C6-S5, so UART
is the only flashing path on the final board.)

## Bill of materials

Hand-solder friendly: THT for power parts, 0805 SMD for everything small
(hot-air or iron). All SMD on one face if possible so hot air never re-melts
the other side.

| Ref | Part | Value / Spec | Package |
|---|---|---|---|
| U1 | Hi-Link AC-DC | HLK-PM01 (230 VAC → 5 V 600 mA) | THT brick, 34×20×15 mm |
| U2 | LDO | AMS1117-3.3 | SOT-223 |
| U3 | MCU module | WT0132C6-S5 (ESP32-C6) | ESP-12F castellated |
| F1 | Fuse | 500 mA T, 250 V (Littelfuse 392/395) | TR5 radial |
| RV1 | Varistor | 275 VAC MOV (S07K275) | 7 mm disc |
| J1 | Screw terminal | 2-pos, 5.08 mm, Phoenix MKDS 1,5/2 (L/N in) | THT |
| J2 | Screw terminal | 3-pos, 5.08 mm, Phoenix MKDS 1,5/3 (SW1/SW2/COM) | THT |
| J3 | Pin header | 1×6, 1.27 mm (programming — may stay unpopulated) | THT |
| R1, R3, R4 | Resistor | 10 kΩ | 0805 |
| R2, R5, R6 | Resistor | 1 kΩ | 0805 |
| C1 | Aluminium electrolytic | 100 µF / 16 V (5 V rail) | SMD 6.3×5.4 mm |
| C2, C5, C7, C9, C10 | Ceramic X7R | 100 nF / 50 V | 0805 |
| C3 | Ceramic X5R | 10 µF / 16 V (LDO in) | 0805 |
| C4 | Tantalum | 22 µF / 10 V (LDO out — required for AMS1117 stability) | EIA-3528 (B) |
| C6 | Aluminium electrolytic | 47 µF / 16 V (3V3 at module) | SMD Ø4–5 ×5.4 mm — LCSC C3337 (Ø5 can fits the Ø4 footprint by hand) |
| C8 | Ceramic X7R | 1 µF (EN delay) | 0805 |
| LED1 | LED | status (green) | 0805 |
| SW3 | Tactile switch | BOOT (IO9) | SMD (TL3342) |

## PCB (circuit/ESP32C6_Wall_Input_Module.kicad_pcb)

**Fully routed, DRC-clean** (0 errors, 0 unconnected). 45 x 36 mm, 2-layer
1.6 mm FR4. Mains on the top face (hand-routed, 1 mm), SELV signals + ground
on both faces, WT0132C6-S5 on the bottom with the antenna overhanging the
lower edge.

- **Mains (top face):** L/N from J1 -> fuse -> MOV -> HLK, hand-routed at
  1 mm, confined to the top-left primary pocket behind the milled creepage
  slot. L on J1 pin 2, N on J1 pin 1 (crossing-free).
- **SELV routing:** the signal and power nets were autorouted (Freerouting)
  with the mains pre-routed and the antenna keep-outs in place, then the
  ground was poured.
- **Ground:** a B.Cu copper pour on the SELV side, shaped as an L that is
  notched out of the mains pocket. Verified **>=6 mm** from every mains pad
  (worst case 6.09 mm at the varistor live pad). Ground is also fully
  track-connected, so the pour is supplementary (islands auto-removed).
- **Clearances:** default 0.2 mm net clearance; board/slot copper-edge rule
  relaxed to 0.4 mm (JLCPCB-safe) to clear the EN track running beside the
  slot.
- **Remaining:** ~38 silkscreen warnings (auto-placed reference designators
  overlapping pads/copper/edge) - cosmetic only; reposition or hide them in
  the PCB editor before ordering. A `*.mains-only.bak` of the pre-routed
  board is kept alongside.

## Breadboard prototype (XIAO ESP32C6)

The sketch has a compile-time board switch at the top — uncomment
`BOARD_XIAO_ESP32C6` instead of `BOARD_WT0132C6_S5`. Bench wiring, no mains
involved (power over USB-C):

| Function | XIAO pin | GPIO |
|---|---|---|
| SW1 input (switch to GND) | D1 | GPIO1 |
| SW2 input (switch to GND) | D2 | GPIO2 |
| Pair / factory reset | BOOT button | GPIO9 |
| Status LED | onboard yellow LED | GPIO15 |

Internal pull-ups are enough on the bench — just hang two push buttons or a
toggle between D1/D2 and GND. The XIAO's RF-switch handling (triple-click the
BOOT button to toggle internal ceramic vs. external U.FL antenna) is compiled
in automatically, same as the WS2811 controller. GPIO4/GPIO5 are used on the
WT0132C6-S5 build because they aren't broken out on the XIAO headers; the
XIAO uses its onboard LED (GPIO15) while the final board drives IO18.

## Installation (typical retrofit)

1. Kill the breaker. Remove the wall switch; identify permanent L, switched
   wire(s) to the lamp, and N (required!).
2. Wire permanent **N → J1-1**, **L → J1-2** (marked on silk). Join the lamp's L wire to
   permanent L with a Wago lever connector (lamp is now always powered).
3. Reconnect the wall switch (now floating, mains-free) between **J2-1 (SW1)**
   and **J2-3 (COM)**; second switch between **J2-2** and **COM**.
4. Power up — LED blinks while pairing, solid 3 s when joined, then off.
   Hold BOOT 5 s to factory-reset/re-pair.
5. In Home Assistant, automate: `contact_1` *changed* → toggle the Hue lamps.
