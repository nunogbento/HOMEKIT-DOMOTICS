# Zigbee OTA for the ESP32-C6 wall-input module

Once a board is in-wall the J3 serial header is unreachable, so **all** firmware
updates go over the Zigbee mesh via zigbee2mqtt's OTA Upgrade cluster. The first
flashed image already contains the OTA client (`addOTAClient` on EP10), so the
boards are updatable for life.

## How Z2M decides to offer an update
After joining, the device sends a *Query Next Image* request with its
`manufacturerCode` (**0x1001**), `imageType` (**0x1011**) and current
`fileVersion`. With `ota: true` in the converter, Z2M looks these up in its OTA
index and offers an image **only if its `fileVersion` is strictly higher**. It
then streams the `.ota` over the mesh (~223-byte blocks); the ESP writes it to
the spare OTA partition and reboots into it.

These three must always agree: the sketch's `OTA_*` constants, the `.ota`
header, and the index entry. They're wired up already — just bump the version.

## One-time Z2M setup (done during first bench test)
1. Deploy the converter: copy `../diy_esp32c6_2ch_input.js` →
   `/home/pi/zigbee2mqtt/data/external_converters/` (Z2M 2.x auto-loads the dir).
2. Create the OTA dir on the Pi and copy this folder's `index.json` + `*.ota`:
   `/home/pi/zigbee2mqtt/data/ota/`  (that maps to `/app/data/ota` inside the
   container — which is what the index `url` fields point to).
3. In `/home/pi/zigbee2mqtt/data/configuration.yaml`, under the existing `ota:`:
   ```yaml
   ota:
     disable_automatic_update_check: true          # already set
     zigbee_ota_override_index_location: ota/index.json
   ```
4. Restart Z2M. (Close irrigation valves is an HA concern, not Z2M — Z2M restart
   is safe, but do it when you can confirm devices rejoin.)

## Publishing a new firmware version (every release)
1. Bump `OTA_RUNNING_FILE_VERSION` in the sketch (e.g. `0x01000000` → `0x01000100`
   for v1.0.1). Rebuild:
   ```
   arduino-cli compile -b "esp32:esp32:esp32c6:ZigbeeMode=zczr,PartitionScheme=zigbee_zczr" \
     --output-dir build ESP32C6_ZIGBEE_Wall_Input_Module.ino
   ```
2. Make the OTA image + refresh the index (replaces the prior entry):
   ```
   python3 ota/make_ota.py --bin build/ESP32C6_ZIGBEE_Wall_Input_Module.ino.bin \
     --version 0x01000100 --out-dir ota --index ota/index.json
   ```
3. Copy the new `.ota` + `index.json` to `/home/pi/zigbee2mqtt/data/ota/`.
4. In the Z2M UI: device → **OTA** tab → *Check for update* → *Update*. (Or wait
   for the hourly auto-query; the device requests within ~1 min of join and hourly.)

## Notes / gotchas
- OTA payload = the **app** binary (`…ino.bin`), NOT the 4 MB `merged.bin`.
  `make_ota.py` refuses anything not starting with `0xE9`.
- The `zigbee_zczr` **4 MB** partition has dual OTA slots; the 2 MB variant does
  NOT — always build with the 4 MB ZCZR scheme.
- **Never wall-mount a board before a proven bench OTA round-trip** (flash v1.0.0
  → push v1.0.1 over the mesh → confirm it boots the new version). It's the one
  thing that can't be fixed after install.
- `url` in the index is the path **as Z2M sees it** (`/app/data/ota/…`). If a
  local path is rejected on your Z2M build, the fallback is `file:///app/data/ota/…`.
