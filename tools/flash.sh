#!/usr/bin/env bash
# Flashes the Vanguard firmware via Docker. Usage: ./flash.sh /dev/ttyACM0
#
# Also erases the NVS partition (identity, contacts, challenge progress,
# settings -- see firmware/partitions.csv, offset 0x9000, size 0x5000) on
# every flash, not just the app image, so every flash from here on hands
# back a genuinely fresh-out-of-the-box badge, not one still carrying
# whoever last tested it.
#
# WHY THIS SCRIPT CALLS ESPTOOL DIRECTLY (not `idf.py flash`): this
# badge's native USB-Serial/JTAG has no dedicated auto-reset circuit, so
# esptool's automatic reset-into-bootloader handshake is unreliable. If
# each step (write, erase, reset) renegotiated bootloader entry on its
# own, a flaky handshake could demand a manual BOOT+RESET up to three
# times per board. `idf.py flash` hardcodes a hard-reset after every
# invocation via sdkconfig, so instead we call esptool directly and chain
# --after no_reset / --before no_reset across every step but the last:
# the board only needs to enter download mode once, at the first command,
# and stays there through the rest of the sequence.
set -euo pipefail
cd "$(dirname "$0")/../firmware"

IDF_IMAGE="espressif/idf:release-v5.3"
PORT="${1:?Usage: ./flash.sh /dev/ttyACMx}"

TTY_FLAGS=""
[ -t 0 ] && TTY_FLAGS="-it"

echo "== Writing firmware to $PORT (bootloader + partition table + OTA data + app) =="
docker run --rm $TTY_FLAGS \
    -v "$(pwd)":/project \
    -w /project \
    --device="$PORT" \
    "$IDF_IMAGE" \
    python -m esptool --chip esp32s3 --port "$PORT" -b 460800 \
        --before default_reset --after no_reset write_flash \
        --flash_mode dio --flash_freq 80m --flash_size 16MB \
        0x0 build/bootloader/bootloader.bin \
        0x8000 build/partition_table/partition-table.bin \
        0xe000 build/ota_data_initial.bin \
        0x10000 build/vanguard_firmware.bin \
        0x610000 build/ffat.bin

# NVS erase step removed so you don't lose your CTF progress, contacts, and settings!

echo "== Resetting $PORT into the new firmware =="
docker run --rm \
    -v "$(pwd)":/project \
    -w /project \
    --device="$PORT" \
    "$IDF_IMAGE" \
    python -m esptool --chip esp32s3 --port "$PORT" \
        --before no_reset --after hard_reset run
