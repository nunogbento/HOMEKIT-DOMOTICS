# Assembly & Bring-up Guide

Hand assembly (hot-air + iron) of the ESP32-C6 Zigbee in-wall input module.
Build order is staged so the **3.3 V rail is proven before the ESP module
goes on**, and the **mains front-end is tested before anything irreplaceable
is at risk**.

> ⚠️ **230 VAC.** The top-left pocket (J1, F1, RV1, U1) is live mains. Do all
> low-voltage bring-up first with a bench supply. Only apply mains through an
> **isolation transformer**, never probe a live board two-handed, and
> **discharge the HLK input** before touching it after power-down.

---

## 0. Tools & consumables

- Hot-air station + fine-tip iron, 0.3–0.5 mm solder, **flux** (essential for
  0805 + the module castellations), tweezers, magnifier/microscope
- **Bench PSU** with current limit (for 5 V injection)
- **Isolation transformer** for mains testing
- Multimeter (continuity + DC volts + ideally capacitance mode)
- USB-UART adapter (3.3 V) for flashing
- IPA + brush for cleaning flux

---

## 1. Component identification

Polarity-critical parts are flagged 🔺. **Tantalum stripe = +, electrolytic
band = −** (opposite conventions — the #1 mistake).

| Ref | Value / spec | Pkg | Side | Marking on part | Polarity |
|---|---|---|---|---|---|
| **U1** | HLK-PM01, 230 VAC→5 V | brick | top | "HLK-PM01" on case; AC L/N one end, +Vo/−Vo other | 🔺 L/N + DC out (silk + pin spacing key it) |
| **U2** | AMS1117-3.3 LDO | SOT-223 | bottom | "AMS1117-3.3" / "1117 33" | 🔺 tab = Vout; orient by tab to silk |
| **U3** | WT0132C6-S5 (ESP32-C6) | ESP-12F | bottom | label on shield | 🔺 orient to silk outline; antenna end overhangs board edge |
| **F1** | 500 mA Time-Lag, 250 V | TR5 | top | "T500mA 250V" (Littelfuse 372) | none (2-lead, one fit) |
| **RV1** | MOV 275 VAC, Ø7 | disc | top | blue disc, "07D431K" | none |
| **C1** | 100 µF 16 V alu | Ø6.3 can | bottom | dark band on top = − | 🔺 band = − |
| **C6** | 47 µF 16 V alu | Ø5 can | bottom | dark band on top = − | 🔺 band = − |
| **C4** | 22 µF 16 V tantalum | 3528-B | bottom | **bar/stripe = +** | 🔺 stripe = + |
| **C3** | 10 µF 25 V X5R | 0805 | bottom | *unmarked* | none |
| **C8** | 1 µF 50 V X7R | 0805 | bottom | *unmarked* | none |
| **C2,C5,C7,C9,C10** | 100 nF 50 V X7R | 0805 | bottom | *unmarked* | none |
| **R1,R3,R4** | 10 kΩ | 0805 | bottom | "103" (or "1002") | none |
| **R2,R5,R6** | 1 kΩ | 0805 | bottom | "102" (or "1001") | none |
| **LED1** | status LED (green) | 0805 | top | cathode mark (line/notch) | 🔺 cathode → R2 side |
| **SW3** | BOOT tactile | SMD | bottom | — | none (symmetric) |
| **J1** | 2-pos screw term (L/N in) | THT | top | — | keyed |
| **J2** | 3-pos screw term (SW1/SW2/COM) | THT | top | — | keyed |
| **J3** | 1×6 prog header, 1.27 mm | THT | top | — | pin 1 = 3V3 (silk) |

> ⚠️ **The three ceramic values (100 nF / 1 µF / 10 µF) are all unmarked 0805
> and look identical.** Keep each in its own labelled strip/bag. If they ever
> get mixed, the only way to tell them apart is a meter in capacitance mode —
> sort before you start.

---

## 2. Pre-assembly checks (bare board)

1. Visual: the internal **slot is milled**, no copper slivers, pads clean.
2. Meter continuity: **no short** 3V3↔GND, 5V↔GND, or L↔N.

---

## 3. Build & test sequence

### Pass 1 — all bottom-side SMD **except U3**
Solder, with flux: **U2, C1, C2, C3, C4, C5, C6, C7, C8, R1–R6, LED1, SW3.**
Mind polarity on **C1, C6 (band = −), C4 (stripe = +)**, and U2 tab to silk.
Leave **U3 off** for now.

> Why U3 last: it's the costliest part and sits over the densest copper —
> proving the rails first means you never cook the module.

### ✅ TEST 1 — 3.3 V rail, **no mains, no ESP**
- Bench PSU to **5.0 V, current limit ~100 mA**.
- Inject **+5 V on the C1 (+) pad**, **GND on any ground pad**.
- Measure at the 3V3 net (e.g. C7 pad / U3 pad 8 land): **expect 3.28–3.33 V.**
- Quiescent draw should be a few mA. If it spikes to the limit → flip board
  off, check U2 orientation and C1/C4/C6 polarity for a reversed/short cap.
- **Do not proceed until 3.3 V is clean.**

### Pass 2 — mains front-end (top, through-hole)
Solder: **J1, F1, RV1, U1**, then **J2** and **J3**.
Keep the primary pocket clean; no flux bridges across the slot.

### ✅ TEST 2 — power rails under mains (**isolation transformer**)
- Lamp loop NOT connected; nothing in J2.
- Apply 230 VAC to **J1: pin 1 = N, pin 2 = L**.
- Measure HLK output ≈ **5 V**, then the **3V3 net ≈ 3.3 V**.
- Power down, **wait for the HLK input cap to discharge**, before handling.
- If no 5 V: check F1, HLK orientation. If 5 V but no 3.3 V: revisit Test 1.

### Pass 3 — the module
Solder **U3 (WT0132C6-S5)** — align to the silk, antenna overhanging the
edge, reflow the castellations with plenty of flux. Inspect pins 8/9 (3V3/GND)
and the side rows for bridges.

### ✅ TEST 3 — powered, with ESP
- Power from bench 5 V (or mains). 3V3 still ≈ 3.3 V under the module's load.
  Back-feeding 5 V into the HLK output with **mains disconnected is safe** —
  the HLK is isolated (3 kVAC) and its output rectifier blocks reverse
  current; just keep polarity correct and never bench-power and mains at once.
- Total draw tens of mA; **status LED (GPIO18) blinks** while searching.
- Excess current or no 3.3 V → check U3 for solder bridges (pins 8↔9 first).

### ✅ TEST 4 — firmware & function
- Flash via **J3** — see *Flashing connector* below for pinout/wiring.
- On boot: LED blinks (joining) → solid when it joins Zigbee → off after 3 s.
- **Inputs:** short **J2 SW1→COM**, then **SW2→COM** — each toggles its
  `contact_*` in Zigbee2MQTT. Both rocker states / momentary presses register.
- **BOOT button:** 5 s hold = factory reset / re-pair.

---

## 3a. Flashing connector (J3)

J3 is a 1×6, 1.27 mm header. **Pin 1 is the square pad** (3V3 end).

| J3 pin | Net | ESP signal | Connect to USB-UART |
|---|---|---|---|
| 1 | **3V3** | module 3V3 | adapter 3V3 — *power option only, see below* |
| 2 | **GND** | GND | adapter **GND** (always) |
| 3 | **TXD0** | GPIO16, ESP **out** | adapter **RX** |
| 4 | **RXD0** | GPIO17, ESP **in** | adapter **TX** |
| 5 | **EN** | reset | tie to GND to reset (manual) |
| 6 | **IO9** | boot strap | tie to GND for download mode (= the BOOT button) |

```
   USB-UART            J3
   ---------           ----
   GND  ───────────────  2  GND
   TX   ───────────────  4  RXD0   (TX→RX, crossed)
   RX   ───────────────  3  TXD0   (RX→TX, crossed)
   3V3  ──(power opt)───  1  3V3
```

**Rules:**
- **3.3 V logic only.** Set the adapter's level jumper to 3V3 — the C6 is not
  5 V tolerant. (CP2102 / CH340 / FT232 modules all have a 3V3 setting.)
- **TX↔RX cross over** (adapter TX → J3 RXD0, adapter RX → J3 TXD0).
- **Powering during flash — pick ONE, never two:**
  - adapter **3V3 → J3 pin 1** (simplest on the bench), **or**
  - bench **5 V** into the board (Test 1 injection point) and leave J3 pin 1
    disconnected — just GND/TX/RX to the adapter.
- ⚠️ **Never flash with mains on J1.** The adapter ground would be tied to
  mains — it will destroy the adapter/PC and is a shock hazard. Flashing is a
  bench, mains-OFF operation. Native USB (GPIO12/13) isn't broken out, so UART
  is the only path.

**Enter download mode** (J3 has no auto-reset DTR/RTS wiring, so it's manual):
1. Hold the **BOOT button** (SW3 — pulls IO9 low). *(or jumper J3 pin 6→GND)*
2. Briefly tie **J3 pin 5 (EN) → GND** and release — *(or power-cycle the board)*.
3. Release BOOT.
4. The C6 ROM is now in download mode — run `esptool` / Arduino "Upload"
   (autobaud, no need to set speed). When done, pulse EN / power-cycle to run.

## 4. Finish

- Clean flux (IPA), re-inspect the mains pocket and the slot for residue.
- Per-board label, then into the wall box. Recheck the [install steps in
  HARDWARE.md](HARDWARE.md): permanent L/N to J1, lamp L bridged with a Wago,
  old switch wires (mains-free) to J2 SW1/SW2/COM.

## Quick polarity recap (the parts that bite)

- **C1, C6** electrolytic cans → **band = negative**.
- **C4** tantalum → **stripe = positive** (opposite!).
- **U2** AMS1117 → **tab/large pad = 3.3 V out**.
- **LED1** → cathode (marked) goes to the R2 side, anode to +3V3.
- **U3** → antenna end over the board edge; match silk.
