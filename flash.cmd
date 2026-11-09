@echo off
rem Build the NodeMCU gate firmware and upload it to the board.
rem   flash            build + upload
rem   flash monitor    build + upload, then open the serial monitor (Ctrl+C to quit)
rem   flash dev        same, using the nodemcu_dev build (serial pass input, LAN http)
rem The USB port is found automatically. Close any other Serial Monitor first.
setlocal
cd /d "%~dp0"

set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
if not exist "%PIO%" (
    echo PlatformIO not found at %PIO%
    echo Install the PlatformIO IDE extension in VS Code, then run this again.
    exit /b 1
)

if not exist "include\secrets.h" (
    echo include\secrets.h is missing. Copy include\secrets.example.h to include\secrets.h and fill it in.
    exit /b 1
)

set "ENV=nodemcu"
set "MONITOR="
for %%A in (%*) do (
    if /i "%%A"=="dev" set "ENV=nodemcu_dev"
    if /i "%%A"=="monitor" set "MONITOR=1"
)

echo === Building and uploading [%ENV%] ===
"%PIO%" run -e %ENV% -t upload
if errorlevel 1 (
    echo.
    echo Upload failed. If it says "Access is denied", close the Serial Monitor
    echo in Arduino IDE ^(or any other program using the port^) and run flash again.
    exit /b 1
)

echo.
echo === Upload finished ===
if defined MONITOR "%PIO%" device monitor -b 115200
