#!/bin/bash
# Upload LittleFS data folder to ESP32-C6
# Partition: min_spiffs (SPIFFS offset: 0x3D0000, size: 0x20000 = 128KB)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="$SCRIPT_DIR/data"
IMAGE_FILE="$SCRIPT_DIR/littlefs.bin"

# Tool paths
MKLITTLEFS="/Users/nunobento/Library/Arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs"
ESPTOOL="/Users/nunobento/Library/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool"

# Partition settings for min_spiffs
SPIFFS_OFFSET="0x3D0000"
SPIFFS_SIZE="0x20000"  # 128KB

# LittleFS block/page size for ESP32
BLOCK_SIZE=4096
PAGE_SIZE=256

# Serial port (auto-detect or specify)
PORT="${1:-/dev/cu.usbserial-*}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== LittleFS Upload Script ===${NC}"
echo ""

# Check data folder exists
if [ ! -d "$DATA_DIR" ]; then
    echo -e "${RED}Error: Data folder not found at $DATA_DIR${NC}"
    exit 1
fi

# List files to upload
echo -e "${GREEN}Files to upload:${NC}"
ls -la "$DATA_DIR"
echo ""

# Check tools exist
if [ ! -f "$MKLITTLEFS" ]; then
    echo -e "${RED}Error: mklittlefs not found at $MKLITTLEFS${NC}"
    exit 1
fi

if [ ! -f "$ESPTOOL" ]; then
    echo -e "${RED}Error: esptool.py not found at $ESPTOOL${NC}"
    exit 1
fi

# Find serial port
if [[ "$PORT" == *"*"* ]]; then
    PORT=$(ls /dev/cu.usbserial-* /dev/cu.SLAB* /dev/cu.wchusbserial* 2>/dev/null | head -1)
fi

if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then
    echo -e "${RED}Error: No serial port found${NC}"
    echo "Available ports:"
    ls /dev/cu.* 2>/dev/null | grep -v Bluetooth
    echo ""
    echo "Usage: $0 /dev/cu.your-port"
    exit 1
fi

echo -e "${GREEN}Using port: $PORT${NC}"
echo ""

# Build LittleFS image
echo -e "${YELLOW}Building LittleFS image...${NC}"
"$MKLITTLEFS" -c "$DATA_DIR" -p $PAGE_SIZE -b $BLOCK_SIZE -s $((SPIFFS_SIZE)) "$IMAGE_FILE"

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to create LittleFS image${NC}"
    exit 1
fi

echo -e "${GREEN}Image created: $IMAGE_FILE ($(du -h "$IMAGE_FILE" | cut -f1))${NC}"
echo ""

# Upload to device
echo -e "${YELLOW}Uploading to ESP32-C6...${NC}"
echo "Press the BOOT button on your device if upload fails to start"
echo ""

"$ESPTOOL" --chip esp32c6 --port "$PORT" --baud 921600 \
    write_flash $SPIFFS_OFFSET "$IMAGE_FILE"

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}Upload successful!${NC}"
    echo "Reset your device to use the new filesystem."

    # Cleanup
    rm -f "$IMAGE_FILE"
else
    echo ""
    echo -e "${RED}Upload failed!${NC}"
    echo "Try:"
    echo "  1. Hold BOOT button while pressing RESET"
    echo "  2. Run the script again"
    exit 1
fi
