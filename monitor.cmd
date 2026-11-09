@echo off
rem Open the serial monitor on the gate board without building or uploading.
rem   monitor          auto-detect the USB port
rem   monitor COM5     use a specific port
rem Type commands (provision, wipe, cancel, rfid, overrides) and press Enter.
rem Ctrl+C to quit. Close it before running flash, or the upload is refused.
setlocal
cd /d "%~dp0"

set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not exist "%PIO%" (
    echo PlatformIO not found at %PIO%
    echo Install the PlatformIO IDE extension in VS Code, then run this again.
    exit /b 1
)

if "%~1"=="" (
    "%PIO%" device monitor -b 115200 --echo
) else (
    "%PIO%" device monitor -b 115200 --echo -p %1
)
