# ESP32 Doorbell — Comelit intercom → Home Assistant → HomeKit

A tiny, fast notifier that turns a legacy **Comelit MT VCC 01** building intercom
into a **family-wide HomeKit doorbell**. When the flat is rung, every family
member's Apple device gets the native HomeKit "doorbell" notification —
anywhere, via the Home hub.

```
 ┌──────────────┐  ring   ┌─────────────┐  HTTP POST   ┌───────────────────────┐
 │ Comelit       │ signal  │ ESP32-C3     │  webhook     │ Home Assistant         │
 │ intercom +    ├────────►│ interceptor  ├─────────────►│  webhook → automation  │
 │ optocoupler   │  power  │ (this board) │  (IoT VLAN)  │  → event.doorbell      │
 └──────────────┘         └─────────────┘              └──────────┬────────────┘
                                                                   │ linked_doorbell_sensor
                                                          ┌────────▼────────────┐
                                                          │ HomeKit "Doorbell    │
                                                          │ Bridge" (camera) →   │
                                                          │ push to whole family │
                                                          └─────────────────────┘
```

## How it works

### The intercom (Comelit MT VCC 01)
"Video door entry with **traditional cabling**" — a non-bus, multi-wire system
with **baseband CVBS video on coax** and analog 2-wire audio.

**Key quirk:** the interceptor is powered from the **central intercom bus**, which
energizes whenever *anyone in the building* is rung and then stays up for a few
minutes. **Power presence is not a ring for our flat** — only a transition of the
**ring signal line** is. The bell can also be pressed several times per power
window.

### The interceptor + board
An optocoupler on the ringing line both powers and signals an **ESP32-C3**
(WiFi Mini). The board:

1. Cold-boots when the bus energizes and connects to Wi-Fi **fast** — static
   nothing fancy, just a cached BSSID/channel in NVS so it skips the scan after
   the first join.
2. Watches the ring line and fires **one notification per ring** (a debounced
   `idle→active` edge; an analog ring is a *buzz burst*, so a 1.5 s release gap
   collapses each press into a single event). **It does not notify on boot.**
3. POSTs to a Home Assistant webhook (with retries — the first POST right after a
   cold Wi-Fi join often needs them), and stays awake for the rest of the window
   to catch repeat presses.

| C3 pin | Connect to |
|---|---|
| `GPIO3` | interceptor ring signal (active-low) |
| `GPIO9` (BOOT button) | built-in **manual test** trigger — press = fake ring |
| `G` | interceptor GND |
| `5V` / `3V3` | interceptor regulated output (match its voltage) |

Firmware: [`firmware/Doorbell_C3_Webhook/Doorbell_C3_Webhook.ino`](firmware/Doorbell_C3_Webhook/Doorbell_C3_Webhook.ino)

### Home Assistant
[`ha/doorbell.yaml`](ha/doorbell.yaml) (deploy as `/config/packages/doorbell.yaml`):

- **Webhook trigger** → `mqtt.publish doorbell/ring` → **`event.doorbell`** (an MQTT
  `event` entity, `device_class: doorbell`). `mode: queued` so rapid rings each fire.

### HomeKit
HA's HomeKit bridge has **no standalone doorbell accessory** — a doorbell only
exists as a **Camera** accessory's `linked_doorbell_sensor` (it does accept an
`event.` entity). So a dedicated YAML bridge **"Doorbell Bridge"** exposes a
`camera.doorbell` with:

```yaml
entity_config:
  camera.doorbell:
    linked_doorbell_sensor: event.doorbell
```

The intercom is audio-only today, so `camera.doorbell` is a **Local File**
camera showing a placeholder image. It's a swap point: digitize the Comelit CVBS
feed later and repoint that same camera — nothing else changes.

## Setup

### 1. Firmware
```bash
# secrets are gitignored — copy the template and fill it in
cp firmware/Doorbell_C3_Webhook/secrets.example.h firmware/Doorbell_C3_Webhook/secrets.h
# edit secrets.h: WIFI_SSID, WIFI_PASS, HA_WEBHOOK_URL

# build + flash (ESP32-C3, USB-CDC serial)
arduino-cli compile --upload -p /dev/cu.usbmodemXXX \
  --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc firmware/Doorbell_C3_Webhook
```
There is **no OTA** (the board is only powered during a call) — reflash on the
bench over USB.

### 2. Home Assistant
1. Copy `ha/doorbell.yaml` to `/config/packages/doorbell.yaml` (requires
   `homeassistant: packages: !include_dir_named packages`).
2. Replace `CHANGE_ME_webhook_id` with a private id (e.g. `openssl rand -hex 8`)
   and use the same value in the device's `secrets.h` `HA_WEBHOOK_URL`.
3. Set the notify/camera entities to match your install.
4. Add a **Local File** camera named `Doorbell` (→ `camera.doorbell`) pointing at a
   placeholder image (e.g. `/config/www/doorbell.jpg`).
5. Restart HA, then **pair "Doorbell Bridge"** in the Home app and enable
   notifications on the Doorbell per device.

## Secrets / publishing
This repo is public. Real values never get committed:
- `secrets.h` is **gitignored**; only `secrets.example.h` (placeholders) is tracked.
- The webhook id in `ha/doorbell.yaml` is a placeholder (`CHANGE_ME_webhook_id`).
- Wi-Fi credentials and the HA URL live only in the local `secrets.h`.

## Roadmap
- **Phase 2 — two-way audio:** bridge the Comelit analog audio pair with an
  I2S-capable ESP32 (the hard part is analog coupling/levels + full-duplex echo).
- **Phase 3 — video:** tap the baseband CVBS coax → CVBS-to-IP encoder (or USB
  capture) → RTSP → repoint `camera.doorbell` for a true HomeKit *video* doorbell.

## Layout
```
firmware/Doorbell_C3_Webhook/   ESP32-C3 firmware (Arduino)
  Doorbell_C3_Webhook.ino
  secrets.example.h             template — copy to secrets.h (gitignored)
ha/doorbell.yaml                Home Assistant package (webhook + event + HomeKit bridge)
enclosure/                      3D-printable enclosure
```
