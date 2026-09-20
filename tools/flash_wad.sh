#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../firmware"

mkdir -p assets/fat
WAD_PATH="assets/fat/doom1.wad"

if [ ! -f "$WAD_PATH" ]; then
    echo "Downloading doom1.wad shareware..."
    curl -L -o "$WAD_PATH" "https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad"
else
    echo "doom1.wad already exists at $WAD_PATH"
fi

echo "WAD is ready!"
echo "The FAT partition image will be automatically built and flashed when you run:"
echo "  ./tools/build.sh"
echo "  ./tools/flash.sh /dev/ttyACM0"
