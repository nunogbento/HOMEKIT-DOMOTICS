#!/bin/bash
# Build and optionally upload HSPCCWDplus sketch
# Uses min_spiffs partition for OTA support (HomeSpan recommended)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH="$SCRIPT_DIR/HSPCCWDplus.ino"

# Board configuration
# CDCOnBoot=cdc enables USB Serial output on ESP32-C6
FQBN="esp32:esp32:esp32c6:PartitionScheme=min_spiffs,FlashSize=4M,CDCOnBoot=cdc"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Parse arguments
UPLOAD=false
PORT=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -u|--upload)
            UPLOAD=true
            shift
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -u, --upload     Upload after compile"
            echo "  -p, --port PORT  Serial port for upload"
            echo "  -h, --help       Show this help"
            exit 0
            ;;
        *)
            PORT="$1"
            shift
            ;;
    esac
done

echo -e "${YELLOW}=== HSPCCWDplus Build Script ===${NC}"
echo "Board: ESP32-C6"
echo "Partition: min_spiffs (1.9MB app + 128KB SPIFFS + OTA)"
echo ""

# Compile
echo -e "${YELLOW}Compiling...${NC}"
arduino-cli compile --fqbn "$FQBN" "$SKETCH" --warnings default

if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Compilation successful!${NC}"

# Upload if requested
if [ "$UPLOAD" = true ]; then
    # Find port if not specified
    if [ -z "$PORT" ]; then
        PORT=$(ls /dev/cu.usbserial-* /dev/cu.SLAB* /dev/cu.wchusbserial* 2>/dev/null | head -1)
    fi

    if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then
        echo -e "${RED}Error: No serial port found${NC}"
        echo "Available ports:"
        ls /dev/cu.* 2>/dev/null | grep -v Bluetooth
        echo ""
        echo "Usage: $0 -u -p /dev/cu.your-port"
        exit 1
    fi

    echo ""
    echo -e "${YELLOW}Uploading to $PORT...${NC}"
    arduino-cli upload --fqbn "$FQBN" --port "$PORT" "$SKETCH"

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Upload successful!${NC}"
    else
        echo -e "${RED}Upload failed!${NC}"
        exit 1
    fi
fi

echo ""
echo "Next steps:"
echo "  - To upload sketch: ./build.sh -u"
echo "  - To upload web files: ./upload_littlefs.sh"
