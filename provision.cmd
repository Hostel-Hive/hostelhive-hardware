@echo off
rem Write a student's pass to an RFID card (no build, no upload, no reset).
rem   provision HH1:<64 hex>          then tap the card within 20 s
rem   provision HH1:<64 hex> COM5     use a specific port
rem   provision wipe                  erase the HostelHive data from the next card
rem Close the Serial Monitor first.
setlocal
cd /d "%~dp0"

set "PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%PY%" (
    echo PlatformIO Python not found at %PY%
    echo Install the PlatformIO IDE extension in VS Code, then run this again.
    exit /b 1
)

"%PY%" -I "%~dp0tools\provision.py" %*
