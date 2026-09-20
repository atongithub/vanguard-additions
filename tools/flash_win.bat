@echo off
set PORT=%1
if "%PORT%"=="" (
    echo Usage: flash_win.bat COMx
    exit /b 1
)

cd %~dp0\..\firmware_vanguard_backup

echo == Writing firmware to %PORT% ==
python -m esptool --chip esp32s3 --port %PORT% -b 460800 ^
    --before default-reset --after no-reset write-flash ^
    --flash-mode dio --flash-freq 80m --flash-size 16MB ^
    0x0 build\bootloader\bootloader.bin ^
    0x8000 build\partition_table\partition-table.bin ^
    0xe000 build\ota_data_initial.bin ^
    0x10000 build\vanguard_firmware.bin

echo == Resetting %PORT% into the new firmware ==
python -m esptool --chip esp32s3 --port %PORT% ^
    --before no-reset --after hard-reset run
