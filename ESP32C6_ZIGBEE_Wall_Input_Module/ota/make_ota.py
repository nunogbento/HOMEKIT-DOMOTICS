#!/usr/bin/env python3
"""
Build a Zigbee OTA Upgrade image (.ota) from a compiled ESP32-C6 app binary and
emit / update the zigbee2mqtt override OTA index entry for it.

WHY: once the board is in-wall the J3 serial header is unreachable, so every
future firmware update ships over the Zigbee mesh via zigbee2mqtt's OTA cluster.
Z2M offers an image only when its fileVersion is HIGHER than what the device
reports, so bump OTA_RUNNING_FILE_VERSION in the sketch each release and rebuild.

INPUT  = the *app* binary (…ino.bin — NOT the 4 MB merged.bin). Zigbee OTA writes
         the app image to the passive OTA partition; bootloader/partition table
         are untouched, so only the app goes in the OTA payload.

These header values MUST match the firmware (addOTAClient params) and the index:
  manufacturer code = 0x1001, image type = 0x1011.

Usage:
  python3 make_ota.py --bin path/to/…ino.bin --version 0x01000000 \
      [--out-dir .] [--manuf 0x1001] [--image-type 0x1011] \
      [--header-string ESP32C6-2CH-INPUT] [--index index.json] \
      [--url-base /app/data/ota]
Writes:  <out-dir>/ESP32C6-2CH-INPUT_v<major.minor.patch>_0x<ver>.ota
Updates: <index> (creates if missing) — replaces any prior entry for this
         manufacturerCode+imageType, keeping it a single current image.
"""
import argparse, hashlib, json, os, struct, sys

OTA_FILE_IDENTIFIER = 0x0BEEF11E
OTA_HEADER_VERSION  = 0x0100
ZIGBEE_STACK_PRO    = 0x0002
TAG_UPGRADE_IMAGE   = 0x0000

def build_ota(app: bytes, manuf: int, image_type: int, file_version: int, header_string: str) -> bytes:
    hs = header_string.encode("ascii")[:32].ljust(32, b"\x00")
    header_len = 4 + 2 + 2 + 2 + 2 + 2 + 4 + 2 + 32 + 4  # = 56, no optional fields
    total_size = header_len + 6 + len(app)               # + subelement (tag2+len4) + payload
    header = struct.pack(
        "<IHHHHHIH32sI",
        OTA_FILE_IDENTIFIER,   # I  upgrade file id
        OTA_HEADER_VERSION,    # H  header version
        header_len,            # H  header length
        0x0000,                # H  field control (no optional fields)
        manuf,                 # H  manufacturer code
        image_type,            # H  image type
        file_version,          # I  file version
        ZIGBEE_STACK_PRO,      # H  zigbee stack version
        hs,                    # 32s header string
        total_size,            # I  total image size
    )
    subelement = struct.pack("<HI", TAG_UPGRADE_IMAGE, len(app)) + app
    return header + subelement

def ver_str(v: int) -> str:
    return f"{(v>>24)&0xFF}.{(v>>16)&0xFF}.{(v>>8)&0xFF}.{v&0xFF}"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True, help="compiled app binary (…ino.bin)")
    ap.add_argument("--version", required=True, help="file version, e.g. 0x01000000 (must equal the sketch's OTA_RUNNING_FILE_VERSION)")
    ap.add_argument("--out-dir", default=".")
    ap.add_argument("--manuf", default="0x1001")
    ap.add_argument("--image-type", default="0x1011")
    ap.add_argument("--header-string", default="ESP32C6-2CH-INPUT")
    ap.add_argument("--model-id", default="ESP32C6-2CH-INPUT")
    ap.add_argument("--index", default="index.json")
    ap.add_argument("--url-base", default="/app/data/ota", help="dir (as Z2M sees it) that will hold the .ota; used to build the index 'url'")
    a = ap.parse_args()

    manuf = int(a.manuf, 0); image_type = int(a.image_type, 0); version = int(a.version, 0)
    app = open(a.bin, "rb").read()
    if app[:1] != b"\xE9":
        sys.exit(f"ERROR: {a.bin} does not start with 0xE9 (not an ESP app image?). Use the …ino.bin, not merged.bin.")

    ota = build_ota(app, manuf, image_type, version, a.header_string)
    os.makedirs(a.out_dir, exist_ok=True)
    fname = f"{a.model_id}_v{ver_str(version)}_0x{version:08x}.ota"
    fpath = os.path.join(a.out_dir, fname)
    open(fpath, "wb").write(ota)
    sha = hashlib.sha512(ota).hexdigest()

    entry = {
        "fileName": fname,
        "fileVersion": version,
        "fileSize": len(ota),
        "url": f"{a.url_base.rstrip('/')}/{fname}",
        "imageType": image_type,
        "manufacturerCode": manuf,
        "sha512": sha,
        "otaHeaderString": a.header_string,
        "modelId": a.model_id,
    }
    # keep a single current entry per (manufacturerCode, imageType)
    idx = []
    if os.path.exists(a.index):
        try: idx = json.load(open(a.index))
        except Exception: idx = []
    idx = [e for e in idx if not (e.get("manufacturerCode") == manuf and e.get("imageType") == image_type)]
    idx.append(entry)
    json.dump(idx, open(a.index, "w"), indent=2)

    print(f"OTA  : {fpath}  ({len(ota)} bytes, app {len(app)} bytes)")
    print(f"ver  : {ver_str(version)}  (0x{version:08x})   manuf 0x{manuf:04x}  imageType 0x{image_type:04x}")
    print(f"sha  : {sha[:32]}…")
    print(f"index: {a.index}  ({len(idx)} entr{'y' if len(idx)==1 else 'ies'})")

if __name__ == "__main__":
    main()
