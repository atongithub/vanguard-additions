# NOTE: GLOB is evaluated at CMake *configure* time, and ESP-IDF runs this
# file in script mode where CONFIGURE_DEPENDS is rejected -- so newly added
# .cpp files are NOT picked up automatically. After adding a source file,
# either touch this CMakeLists.txt or run a clean build, otherwise the# files compiles into nothing and you get a confusing link-time "undefined
# reference" rather than a missing-file error.
# Touched for Image Viewer addition.
file(GLOB_RECURSE VANGUARD_SOURCES "*.cpp")

# Boot animation frames (see tools/gen_boot_anim.py) -- embedded as raw
# linked binary blobs via EMBED_FILES, not compiled-in C hex arrays, so the
# ~590KB of JPEG data doesn't bloat compile time or source size. Same
# GLOB-at-configure-time caveat as VANGUARD_SOURCES above: re-running
# gen_boot_anim.py with a different frame count needs a `touch` of this
# file (or a clean build) to be picked up.
file(GLOB BOOT_ANIM_FRAME_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "boot_anim_frames/*.jpg")

idf_component_register(
    SRCS ${VANGUARD_SOURCES}
    INCLUDE_DIRS "." "rf" "display" "storage" "power" "input" "leds" "audio"
                 "ui" "perf" "settings" "contacts" "music" "boot" "games"
                 "challenges" "satellite" "radiochat" "glitch" "girlfriend" "../include"
    EMBED_FILES ${BOOT_ANIM_FRAME_FILES}
    REQUIRES espressif__arduino-esp32 jgromes__radiolib driver esp_timer
             Adafruit_GFX_Library Adafruit_ST7735_and_ST7789_Library
             nvs_flash h2zero__esp-nimble-cpp mbedtls espressif__qrcode
             espressif__esp_jpeg wear_levelling fatfs
    # Without this, static-archive linking only pulls in object files that
    # resolve a symbol the linker is *already* looking for at scan time --
    # with 80 EMBED_FILES-generated frame objects sharing this one archive
    # with boot.cpp.o (the only referencer), roughly half of them were
    # silently left out ("undefined reference" for frame0044 onward, while
    # 0001-0043 linked fine) depending on archive member order. WHOLE_ARCHIVE
    # forces every object in this component into the final link
    # unconditionally, sidestepping that resolution-order dependency.
    WHOLE_ARCHIVE
)

fatfs_create_spiflash_image(ffat ../assets/fat FLASH_IN_PROJECT)
# Optional satellite-simulator build of this same firmware:
#     idf.py -DVANGUARD_SATELLITE_SIM=1 build
# Produces an image that acts as the Vanguard satellite (Mission 02
# telemetry, Mission 03 uplink authentication, Mission 04 payload downlink)
# rather than a player badge. Kept in-tree deliberately so the two sides of
# the radio protocol cannot drift apart.
if(VANGUARD_SATELLITE_SIM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE VANGUARD_SATELLITE_SIM=1)
endif()
