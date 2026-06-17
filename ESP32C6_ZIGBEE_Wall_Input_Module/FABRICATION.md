# Fabrication — JLCPCB

Upload **`circuit/ESP32C6_Wall_Input_Module_JLCPCB.zip`** to https://jlcpcb.com/quote
(Add gerber file → it auto-detects the stack-up and the internal slot).

Generated with `kicad-cli` from the routed, DRC-clean board (0 errors).
Gerbers + Excellon drill live in `circuit/fab/`.

## Order settings

| Field | Value |
|---|---|
| Base material | FR-4 |
| Layers | 2 |
| Dimensions | 45.1 × 36.1 mm (single piece, no panel) |
| PCB thickness | 1.6 mm |
| Surface finish | HASL lead-free (ENIG if you prefer for hot-air SMD) |
| Outer copper | 1 oz |
| Min hole | 0.3 mm (vias) — standard, no surcharge |
| Min track/space | ~0.2 mm used — well inside JLC capability |
| Castellated / edge plating | No |
| Via covering | Tented (default) |

## Important

- **The rectangular cut-out is intentional** — it's the mains↔SELV creepage
  slot. JLC will flag the internal `Edge.Cuts` contour during review; confirm
  it as a **slot/cut-out**, do not fill it.
- This is a **230 VAC board**. Verified ≥6 mm copper creepage primary→SELV.
- Through-hole power parts (HLK, terminals, fuse, MOV) + 0805/SOT-223 SMD on
  the bottom — hand/hot-air assembly. No JLC PCBA needed (order bare boards;
  parts come from LCSC in the same cart, see BOM in HARDWARE.md).

## Files in the zip

F.Cu, B.Cu, F/B.Mask, F/B.Silkscreen, F/B.Paste, Edge.Cuts (Gerber RS-274X),
plus `.drl` (Excellon, mm). Paste layers are only needed if you also order a
stencil.
