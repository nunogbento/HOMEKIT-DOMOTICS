# Zigbee OTA for the ESP32-C6 wall-input module

Once a board is in-wall the J3 serial header is unreachable, so **all** firmware
updates go over the Zigbee mesh via zigbee2mqtt's OTA Upgrade cluster. The first
flashed image already contains the OTA client (`addOTAClient` on EP10), so the
boards are updatable for life.

Deployed and proven end-to-end: board 1 was OTA-updated in-wall from v3
(`0x01000002`) to v8 (`0x01000007`) over the mesh, rollback-guarded.

## How Z2M decides to offer an update

After joining, the device sends a *Query Next Image* request with its
`manufacturerCode` (**0x1001**), `imageType` (**0x1011**) and current
`fileVersion`. With `ota: true` in the converter, Z2M looks these up in its OTA
override index and offers an image **only if its `fileVersion` is strictly
higher**. It then streams the `.ota` over the mesh (~223-byte blocks); the ESP
writes it to the spare OTA partition and reboots into it.

Three things must always agree, and they're wired up already — just bump the
version each release:
- the sketch's `OTA_FW_RUNNING` constant,
- the `.ota` header `fileVersion` (the `--version` arg to `make_ota.py`),
- the `fileVersion` in the index entry (`make_ota.py` writes it).

### Version scheme
`0xMMmmpprr` (major.minor.patch.build). This project increments the **last
byte** per release: v3=`0x01000002`, v4=`0x01000003`, … v7=`0x01000006`,
**v8=`0x01000007`** (current). `fileVersion` in the index is that value in
decimal (v8 = `16777223`).

## Where things live (deployed layout on the Pi, `pi@192.168.1.109`)

Z2M data dir = `/home/pi/zigbee2mqtt/data` (mounted as `/app/data` in the
`zigbee2mqtt` container). The OTA override lives under it:

```
data/external_converters/diy_esp32c6_2ch_input.js   # the converter (ota: true)
data/ota_override/index.json                         # the override index
data/ota_override/ESP32C6-2CH-INPUT_v0x01000007.ota  # the image(s)
```

`data/configuration.yaml`:
```yaml
ota:
  zigbee_ota_override_index_location: ota_override/index.json   # the key Z2M uses
  zigbee_ota_override_index_file: ota_override/index.json       # legacy alias, harmless
```
The index `url` is **relative to the data dir** (`ota_override/…`), which is why
`make_ota.py` defaults `--url-base ota_override`.

## Publishing a new firmware version (every release)

1. Bump `OTA_FW_RUNNING` in the sketch (e.g. `0x01000007` → `0x01000008`).
   `OTA_FW_DOWNLOADED` follows as `RUNNING + 1`. Rebuild:
   ```
   arduino-cli compile -b "esp32:esp32:esp32c6:PartitionScheme=zigbee_zczr,ZigbeeMode=zczr" \
     --output-dir build ESP32C6_ZIGBEE_Wall_Input_Module.ino
   ```
2. Make the OTA image + refresh the index (replaces the prior entry; the
   `--version` MUST equal the sketch's `OTA_FW_RUNNING`):
   ```
   python3 ota/make_ota.py --bin build/ESP32C6_ZIGBEE_Wall_Input_Module.ino.bin \
     --version 0x01000008 --out-dir ota --index ota/index.json
   ```
   This writes `ota/ESP32C6-2CH-INPUT_v0x01000008.ota` + updates `ota/index.json`.
3. Deploy both to the Pi (the repo `ota/` mirrors `data/ota_override/`):
   ```
   scp ota/ESP32C6-2CH-INPUT_v0x01000008.ota ota/index.json \
     pi@192.168.1.109:/home/pi/zigbee2mqtt/data/ota_override/
   ```
4. Restart Z2M so it reloads the override index:
   `ssh pi@192.168.1.109 'docker restart zigbee2mqtt'`.
5. Trigger the update. Either the Z2M UI (device → **OTA** tab → *Check for
   update* → *Update*), or over MQTT (broker `192.168.1.109:1883`, anonymous):
   ```
   # check
   mosquitto_pub -h 192.168.1.109 -t zigbee2mqtt/bridge/request/device/ota_update/check \
     -m '{"id":"In Wall dual switch 1"}'
   # update (long-running; progress on zigbee2mqtt/<device> .update.progress)
   mosquitto_pub -h 192.168.1.109 -t zigbee2mqtt/bridge/request/device/ota_update/update \
     -m '{"id":"In Wall dual switch 1"}'
   ```

## Anti-brick: rollback + why you still bench-test first

**OTA is NOT crash-resistant.** A device that crash-loops restarts the download
from block 0 every reboot and never finishes — the mesh can't recover it, only a
wired reflash can. Two defenses, both in place:

- **App rollback (in firmware since v6).** The sketch defines
  `verifyRollbackLater() → true`, so an OTA'd image boots as *pending-verify*;
  only after it stays joined and passes a health check (`OTA_VALIDATE_MS`, ~90 s)
  does it call `esp_ota_mark_app_valid_cancel_rollback()`. If the new image
  faults before that, the bootloader reverts to the previous image. This is what
  makes an in-wall OTA safe. (Verified on board 1's v3→v8: it validated, no
  revert.) Requires the 4 MB `zigbee_zczr` dual-slot partition.
- **Bench-test every image before it ships to an in-wall board.** Flash it to a
  bench board (ideally the SELV-only bench board, no mains), confirm it joins and
  is stable, THEN OTA the wall units. Rollback saves a bad *boot*; it does not
  save a bad image that boots fine but is wrong.

## Timing
The ~740 KB image took ≈**69 min** over the mesh for board 1 (rate rose from
~0.7 %/min to ~1.6 %/min). Keep the board powered; in-wall on AC this is a
non-issue. Don't interpret a slow % as a hang.

## Notes / gotchas
- OTA payload = the **app** binary (`…ino.bin`), NOT the 4 MB `merged.bin`.
  `make_ota.py` refuses anything not starting with `0xE9`.
- The `zigbee_zczr` **4 MB** partition has dual OTA slots (and the rollback
  region); the 2 MB variant does NOT — always build with the 4 MB ZCZR scheme.
- The **repo `ota/` dir mirrors the Pi's `ota_override/`** — same filenames,
  same `index.json`. Keep them in sync (step 3 above).
- Do NOT add `bind` + `configureReporting` for the temperature/analog clusters
  in the converter's `configure()` — the coordinator's ConfigureReporting write
  crashes the device's stack (see the repo's firmware header and the
  `esp32c6-zcl-report-crash` note). Diagnostics are polled by read, not reported.
