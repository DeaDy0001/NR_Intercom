@echo off
rem ==========================================================================
rem  NR Intercom - ESP32-S3 AMOLED Display-Firmware flashen (Windows)
rem
rem  Nutzt esptool aus der Arduino IDE (Paket "esp32 by Espressif").
rem  Einfach doppelklicken, COM-Port waehlen, fertig.
rem ==========================================================================
setlocal EnableExtensions EnableDelayedExpansion
title NR Intercom - S3 Display flashen
cd /d "%~dp0"

set "BIN=%~dp0bin\nr-display-s3.bin"
set "CHIP=esp32s3"

echo.
echo  ================================================
echo   NR Intercom - ESP32-S3 Display-Firmware flashen
echo  ================================================
echo.

if not exist "%BIN%" goto :no_bin

rem ---------------------------------------------------------------- esptool suchen
set "ESPCMD="
call :find_in "%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py"
if not defined ESPCMD call :find_in "%USERPROFILE%\Documents\ArduinoData\packages\esp32\tools\esptool_py"
if not defined ESPCMD call :find_in "%USERPROFILE%\AppData\Local\Arduino15\packages\esp32\tools\esptool_py"
if not defined ESPCMD (
    for /f "delims=" %%E in ('where esptool.exe 2^>nul') do if not defined ESPCMD set ESPCMD="%%E"
)
if not defined ESPCMD (
    python -m esptool version >nul 2>&1
    if not errorlevel 1 set "ESPCMD=python -m esptool"
)
if not defined ESPCMD goto :no_esptool
echo  esptool: %ESPCMD%
echo  Datei  : %BIN%
echo.

rem ---------------------------------------------------------------- COM-Port waehlen
:choose_port
echo  Gefundene serielle Geraete:
powershell -NoProfile -Command "$p = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM\d+\)' }; if ($p) { $p | ForEach-Object { '    ' + $_.Name } } else { '    keine gefunden' }"
echo.
set "DEFPORT="
set /a NPORTS=0
for /f "delims=" %%P in ('powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object -Unique"') do (
    set /a NPORTS+=1
    set "DEFPORT=%%P"
)
if !NPORTS! NEQ 1 set "DEFPORT="

set "PORT="
if defined DEFPORT (
    set /p "PORT=  COM-Port eingeben [Enter = !DEFPORT!]: "
    if "!PORT!"=="" set "PORT=!DEFPORT!"
) else (
    set /p "PORT=  COM-Port eingeben, z. B. COM5 oder nur 5: "
)
if "!PORT!"=="" goto :choose_port
echo !PORT!| findstr /i /b "COM" >nul || set "PORT=COM!PORT!"

rem ---------------------------------------------------------------- Flashen
:flash
echo.
echo  Flashe auf !PORT! ...
echo.
%ESPCMD% --chip %CHIP% --port !PORT! --baud 921600 write_flash 0x0 "%BIN%"
if not errorlevel 1 goto :ok

echo.
echo  Schneller Versuch fehlgeschlagen, versuche es langsamer ...
echo.
%ESPCMD% --chip %CHIP% --port !PORT! --baud 115200 write_flash 0x0 "%BIN%"
if not errorlevel 1 goto :ok

echo.
echo  ------------------------------------------------------------
echo   FLASHEN FEHLGESCHLAGEN. Bitte pruefen:
echo    1. Richtiger COM-Port? Serieller Monitor der Arduino IDE
echo       muss GESCHLOSSEN sein, er blockiert den Port.
echo    2. Download-Modus: BOOT-Taste gedrueckt halten, kurz
echo       RESET druecken, BOOT loslassen, dann nochmal versuchen.
echo    3. USB-Kabel mit Datenleitungen, kein reines Ladekabel.
echo  ------------------------------------------------------------
echo.
set "AGAIN="
set /p "AGAIN=  Nochmal versuchen? [j/n]: "
if /i "!AGAIN!"=="j" goto :choose_port
goto :ende

:ok
echo.
echo  ------------------------------------------------------------
echo   FERTIG. Das Display startet neu und zeigt die Oberflaeche.
echo   Falls nicht: einmal RESET druecken.
echo   Log-Ausgaben: serieller Monitor der Arduino IDE, 115200 Baud.
echo  ------------------------------------------------------------
goto :ende

rem ---------------------------------------------------------------- Hilfsfunktionen
:find_in
rem neueste esptool-Version im Ordner %1 suchen (nach Datum sortiert)
if not exist "%~1" exit /b 0
for /f "delims=" %%D in ('dir /b /ad /o:d "%~1" 2^>nul') do (
    if exist "%~1\%%D\esptool.exe" set ESPCMD="%~1\%%D\esptool.exe"
)
exit /b 0

:no_bin
echo  [FEHLER] Firmware-Datei nicht gefunden:
echo           %BIN%
echo  Die Datei flash-s3.bat muss im Ordner "firmware" liegen,
echo  neben dem Unterordner "bin".
goto :ende

:no_esptool
echo  [FEHLER] esptool wurde nicht gefunden.
echo.
echo  Loesung: Arduino IDE oeffnen, Werkzeuge - Board - Boardverwalter,
echo  "esp32 by Espressif" installieren, dann diese Datei erneut starten.
echo  Alternativ ohne Installation im Browser flashen:
echo     https://espressif.github.io/esptool-js/   Datei bei Adresse 0x0
goto :ende

:ende
echo.
pause
endlocal
