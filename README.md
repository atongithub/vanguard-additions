# Vanguard — Portable Hardware Security Laboratory (Custom Additions)

> **Note:** This repository contains custom mods, extra games, companion tools, media pipelines, and quirky features added to the original Vanguard firmware (originally created by Amrita's InCTF team).

Vanguard is a handheld hardware security platform: a self-contained,
battery-powered device built around an ESP32-S3 that doubles as a badge, a
field radio, a games console, and a live embedded-security CTF target. It
is designed for education, hardware-hacking research, wireless
experimentation, and competition deployment — something a participant can
be handed, play with, and hack, all on the same piece of hardware.

Firmware is one subsystem of the broader Vanguard platform: this
repository is the canonical engineering source for that firmware, its
companion tooling, and the documentation needed to build, flash, deploy,
and extend it without depending on any other repository.

## Core capabilities

- **Interactive hardware security challenges** — a four-level, on-device
  CTF spanning UART reconnaissance, RF/protocol reverse engineering,
  authenticated uplink scripting, and file-format forensics.
- **LoRa communications** — a shared physical/link layer used by Radio
  Chat, Ship Battle, and the challenge arc's satellite link.
- **BLE PeerDrop** — badge-to-badge contact exchange over Bluetooth Low
  Energy.
- **Radio Chat** — a handheld LoRa messenger with a real Morse/CW mode.
- **Morse Mode** — manual CW send/receive layered on the Radio Chat link.
- **Ship Battle** — a badge-to-badge game built on the same LoRa link
  layer as Radio Chat.
- **Game Boy Emulation (Pokémon Red)** — embedded Peanut-GB emulator running
  Game Boy ROMs directly from a dedicated flash partition.
- **Doom Terminal Link** — high-speed 5 Mbps UART link streaming Doom frames
  from an external coprocessor with 2x integer scaling.
- **Vanguard Buddy (AI Companion)** — BLE GATT companion communicating with
  local Ollama LLMs with real-time text, voice chirps, and a 14-LED mood meter.
- **MJPEG Video & Media Engine** — custom video boot animation and looping
  video screensaver with speed-boost mode and buzzer melody audio.
- **Novelty Apps & Easter Eggs** — Magic 8-Ball, Pop a Cig, and a secret
  Konami code sequence unlocking "Vajra Mode".
- **Product Website** — complete companion Next.js web application with
  Tailwind CSS and Framer Motion.
- **Logic-analyzer-based exercises** — Level 1 of the challenge arc is a
  hands-on UART sniffing exercise against real hardware.
- **Portable, field-deployable design** — battery-powered, no
  infrastructure dependency beyond the badges themselves.
- **Competition-grade challenge infrastructure** — sequential unlock
  chain, persistent progress, and a companion satellite simulator so both
  sides of the RF protocol ship from one codebase.

## Hardware overview

Vanguard runs on an ESP32-S3-WROOM-1 with a color TFT display, an
addressable LED chain, a joystick + button input cluster, a LoRa radio
module, and buzzer audio. See [`docs/architecture/`](docs/architecture/)
for the full subsystem breakdown and [`firmware/include/pins.h`](firmware/include/pins.h)
for the pin map. The `hardware/` directory tracks schematics, PCB,
enclosure, and manufacturing files as they become available.

## Firmware overview

The firmware is built on **ESP-IDF, with Arduino integrated as an ESP-IDF
component**, and builds entirely inside the official Espressif Docker
image — Docker is the only build prerequisite. `main.cpp` drives an
`AppState` state machine: exactly one screen is active at a time, each
screen implements `enter()`/`frame()`, and subsystem managers (LEDs,
audio, animation, power, challenges) tick independently of whichever
screen is active. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the full
picture.

## Feature summary

| Area | What it does |
|---|---|
| Boot Video & Melody | Hardware/software MJPEG video playback with synchronized chiptune audio |
| Looping Screensaver | 15/30 FPS MJPEG looping video with interactive speed boost and LED sweep |
| Menu system | Central navigation hub with hidden Konami code easter egg |
| Profile Setup / Settings | Per-badge identity, preferences, unlock state |
| Contacts | Stores identities exchanged via PeerDrop |
| Music Player | Chiptune playback with an LED pitch visualizer |
| Games Menu | Tetris, Snake, Space Shooter, 2048, Ship Battle, Doom, Pokémon Red |
| Pokémon Red (Game Boy) | Peanut-GB emulator running Pokémon Red from a dedicated 1MB partition |
| Doom Terminal Link | 5 Mbps UART1 streaming coprocessor link (160x120 RGB565 scaled 2x) |
| Vanguard Buddy | BLE AI companion connected to local Ollama LLM with a 14-LED mood meter |
| Magic 8 Ball | Fortune-teller app with MJPEG animations and witty responses |
| Pop a Cig | Animated smoke break sequence with buzzer rumble effects |
| Picture Viewers | Character display screens (Tsundere Miku, Paarivendhar) |
| Radio Chat / Morse Mode | LoRa handheld messenger with CW send/receive |
| PeerDrop | BLE badge-to-badge contact exchange |
| Challenge Framework | The four-level on-device CTF |
| Storage | NVS and FAT/SPIFFS persistence for identity, ROMs, assets, and progress |
| Product Website | Full Next.js showcase web application with Tailwind CSS and Framer Motion |

---

## Custom Mods & Additions (Workspace Overview)

This workspace contains significant custom extensions built on top of the base Vanguard firmware:

### 1. Game Boy Emulation — Pokémon Red
- **Emulator Core:** Integrated the lightweight [`peanut_gb.h`](firmware/main/games/peanut_gb.h) emulator in [`firmware/main/games/pokemon.cpp`](firmware/main/games/pokemon.cpp).
- **Display Pipeline:** Performs 2x integer scaling from the 160x144 Game Boy resolution up to the badge's 320x240 ST7789 display with custom RGB565 palettes.
- **Flash ROM Storage:** Uses a dedicated 1MB partition `gb_rom` defined at `0xEF0000` in [`firmware/partitions.csv`](firmware/partitions.csv) containing [`pokemon_red.gb`](pokemon_red.gb) memory-mapped directly into the ESP32-S3's address space.
- **Controls:** Direct joypad mapping to the joystick and action buttons.

### 2. Doom High-Speed Coprocessor Link
- **Architecture:** Implemented in [`firmware/main/games/doom_screen.cpp`](firmware/main/games/doom_screen.cpp). Instead of compromising on-badge RAM, Vanguard connects to a secondary coprocessor (e.g. ESP32-S3 N16R8) over UART1 at **5,000,000 baud** (GPIO 43/44).
- **Video & Input Stream:** Streams 160x120 RGB565 frames with the `"DOOM"` magic header, upscaled 2x to 320x240, while packing badge buttons into a 1-byte mask sent back upstream.
- **Asset Helper:** [`tools/flash_wad.sh`](tools/flash_wad.sh) downloads the `doom1.wad` shareware file directly into the firmware FAT asset directory.

### 3. Vanguard Buddy — Local AI Companion over BLE
- **Firmware Peripheral:** [`firmware/main/vanguard_buddy/vanguard_buddy.cpp`](firmware/main/vanguard_buddy/vanguard_buddy.cpp) hosts a BLE GATT peripheral named `Vanguard-Buddy`.
- **Display & Audio:** Displays custom buddy bitmap avatars, word-wrapped messages, and synthesized speech chirps.
- **14-LED Mood Meter:** Evaluates the AI's emotional response on a scale of 0–14 and renders a real-time "Love / Mood Meter" across the badge's 14 addressable LEDs.
- **PC Host Bridge:** [`tools/vanguard_buddy/host_bridge.py`](tools/vanguard_buddy/host_bridge.py) uses Bleak to connect from a PC and stream conversational responses from local Ollama LLMs (e.g. `dolphin-llama3`, `mistral`, `phi3`).
- **Image Converter:** [`tools/vanguard_buddy/convert_image.py`](tools/vanguard_buddy/convert_image.py) to convert PNG/JPG graphics into RGB565 bitmap headers.

### 4. MJPEG Video Engine & Custom Boot / Screensaver
- **Boot Video:** [`firmware/main/boot/boot.cpp`](firmware/main/boot/boot.cpp) decodes MJPEG video frames via `esp_jpeg` directly to the display (`boot_video_data.h`) accompanied by a buzzer melody (`boot_audio_data.h`).
- **Looping Screensaver:** [`firmware/main/ui/screensaver.cpp`](firmware/main/ui/screensaver.cpp) loops an MJPEG car animation (`video_data.h`) with a 15 FPS / 30 FPS dynamic "speed boost" mode and accelerated LED sweeps.
- **Media Conversion Pipeline:**
  - [`tools/convert_media.py`](tools/convert_media.py): Extracts video frames and uses `librosa.pyin` to transcribe `.m4a` audio into pitch/duration tables for the buzzer.
  - [`tools/encode_video.py`](tools/encode_video.py): Standalone CLI tool to convert any MP4 into 160x120 MJPEG C header arrays.
  - [`tools/generate_ino.py`](tools/generate_ino.py) & [`tools/simulator_audio.ino`](tools/simulator_audio.ino): Exports header melodies to an Arduino sketch for buzzer hardware testing.

### 5. Novelty Apps & Easter Eggs
- **Magic 8-Ball:** [`firmware/main/ui/8ball.cpp`](firmware/main/ui/8ball.cpp) with full video roll animation and 30+ witty, sarcastic, and funny answers.
- **Pop a Cig:** [`firmware/main/ui/pop_cig.cpp`](firmware/main/ui/pop_cig.cpp) animated smoke break sequence with custom buzzer rumble effects.
- **Konami Code Easter Egg:** In the main menu ([`firmware/main/ui/menu.cpp`](firmware/main/ui/menu.cpp)), entering `Up, Up, Down, Down, Left, Right, Left, Right, Back, Ok` triggers a popup toast `"VAJRA MODE UNLOCKED"`, an audio jingle, and a rainbow LED sweep.
- **Character / Picture Viewers:** Custom graphic viewers including Tsundere Miku and Paarivendhar screens.

### 6. Companion Web Application (`website/`)
- A public showcase site located in [`website/`](website/) built with **Next.js (App Router)**, **Tailwind CSS**, **Framer Motion**, and **TypeScript**.
- Configured for static HTML export to GitHub Pages, detailing the badge's hardware, architecture, challenges, and user manual.

### 7. Flashing & System Utilities
- **Windows Flashing Script:** [`tools/flash_win.bat`](tools/flash_win.bat) provides one-click native flashing using `esptool.py` directly on Windows without Docker.
- **WSL2 Docker Relocation:** [`move_docker.ps1`](move_docker.ps1) automates exporting and moving Docker Desktop WSL2 virtual disks from the `C:` drive to `D:\DockerData` to prevent disk exhaustion during large firmware builds.

---

## Build instructions

```bash
git clone https://github.com/bi0sHardware/Vanguard-Portable-Hardware-Security-Laboratory.git
cd Vanguard-Portable-Hardware-Security-Laboratory

./tools/build.sh
```

The first build downloads the Espressif Docker image and all managed
components and takes several minutes; subsequent builds are incremental.
No local ESP-IDF install, toolchain, or Python environment is required to
build.

## Flash instructions

### Linux / macOS (Docker)

```bash
./tools/flash.sh /dev/ttyACM0        # flash a badge
./tools/monitor.sh /dev/ttyACM0      # open a serial monitor
./tools/clean.sh                     # full clean
```

### Windows (Native via esptool)

If you are developing natively on Windows:

```cmd
tools\flash_win.bat COMx
```

### Flashing Pokémon Red Game Boy ROM

To write [`pokemon_red.gb`](pokemon_red.gb) into the dedicated `gb_rom` partition (offset `0xEF0000`):

```bash
python -m esptool --chip esp32s3 --port /dev/ttyACM0 write_flash 0xEF0000 pokemon_red.gb
```

For brand-new, never-flashed hardware (which needs a one-time eFuse fix —
see [`docs/deployment/`](docs/deployment/)):

```bash
./tools/provision_new_badge.sh --watch
```

On Linux, if flashing fails with MD5 mismatches or "chip stopped
responding", stop ModemManager first — it probes the serial port and can
trigger reset loops:

```bash
sudo systemctl stop ModemManager
```

## Repository structure

```
vanguard/
├── firmware/                  ESP-IDF firmware source, vendored display libraries, build config
│   ├── assets/                Raw media, video clips, and FAT filesystem sources
│   ├── main/
│   │   ├── boot/              Video boot player and chiptune audio headers
│   │   ├── challenges/        CTF challenge engines and state machines
│   │   ├── games/             Games (Tetris, Snake, Space Shooter, 2048, Doom, Pokémon Red)
│   │   ├── ui/                Menu, screensaver, Magic 8-Ball, Pop a Cig, renderer
│   │   ├── vanguard_buddy/    BLE AI companion peripheral implementation
│   │   └── rf/                SX1262 LoRa driver and radio link layers
│   └── partitions.csv         Partition table including 1MB gb_rom and FAT assets
├── website/                   Next.js + Tailwind CSS + Framer Motion product site
├── tools/
│   ├── vanguard_buddy/        Ollama LLM BLE host bridge and avatar converter
│   ├── convert_media.py       Video/audio extraction pipeline using OpenCV & Librosa
│   ├── encode_video.py        CLI MJPEG video encoder
│   ├── generate_ino.py        Header-to-Arduino melody generator
│   ├── flash_win.bat          Windows native esptool flashing script
│   ├── flash_wad.sh           Doom WAD downloader script
│   └── build.sh / flash.sh    Docker-based build and flash scripts
├── hardware/                  Schematics, PCB, enclosure, and manufacturing files
├── docs/                      Architecture, protocol, challenge, and deployment docs
├── pokemon_red.gb             1MB Pokémon Red ROM for Peanut-GB
├── move_docker.ps1            PowerShell script to move WSL2 Docker data to D:
├── README.md                  This file
├── ARCHITECTURE.md            System architecture
└── LICENSE                    MIT
```

## Supported hardware

ESP32-S3-WROOM-1-based Vanguard badge PCBs, 16MB flash. See
[`docs/architecture/`](docs/architecture/) and
[`firmware/include/pins.h`](firmware/include/pins.h) for the full pinout
and hardware constraints (backlight wiring, LoRa RESET availability, SPI
bus sharing, VDD_SPI eFuse requirement).

## Quick start

1. Install Docker.
2. `git clone` this repository.
3. `./tools/build.sh`
4. `./tools/flash.sh /dev/ttyACM0` (or `tools\flash_win.bat COMx` on Windows)
5. `./tools/monitor.sh /dev/ttyACM0` to watch it boot.

For event/competition deployment, see [`DEPLOYMENT.md`](DEPLOYMENT.md).
For challenge design and progression, see [`docs/challenges/`](docs/challenges/)
(participant-facing, spoiler-free).

## Documentation

- [`ARCHITECTURE.md`](ARCHITECTURE.md), [`docs/architecture/`](docs/architecture/) — system design
- [`website/README.md`](website/README.md) — companion web application setup and deployment
- [`docs/protocols/`](docs/protocols/) — LoRa, Radio Chat, Morse, and PeerDrop wire protocols
- [`docs/challenges/`](docs/challenges/) — challenge framework and per-level design (no flags/solutions)
- [`DEPLOYMENT.md`](DEPLOYMENT.md), [`docs/deployment/`](docs/deployment/) — badge prep and event checklists
- [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md) — build, flash, and hardware failure modes
- [`DEVELOPMENT.md`](DEVELOPMENT.md) — workflow, standards, and how to extend the platform
- [`wiki/`](wiki/) — per-feature reference pages
- [`docs/release/RELEASE-1.0-CERTIFICATION.md`](docs/release/RELEASE-1.0-CERTIFICATION.md) — release readiness status

## License

MIT — see [`LICENSE`](LICENSE).

