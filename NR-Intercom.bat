@echo off
rem ==========================================================================
rem  NR Intercom - Werkzeug-Fenster (Doppelklick)
rem  Flashen, Konsole und Befehle fuer C5 (Bodypack) und S3 (Display).
rem  Im Fenster "help" eingeben.
rem ==========================================================================
setlocal
title NR Intercom
cd /d "%~dp0"

set "PYEXE="
py -c "import esptool, serial" >nul 2>&1 && set "PYEXE=py"
if not defined PYEXE python -c "import esptool, serial" >nul 2>&1 && set "PYEXE=python"
if defined PYEXE goto :start

echo.
echo  Python mit esptool fehlt noch (einmalig noetig).
set "ANS="
set /p "ANS=  Jetzt installieren (py -m pip install esptool)? [j/n]: "
if /i not "%ANS%"=="j" goto :fehlt
py -m pip install esptool || python -m pip install esptool
py -c "import esptool, serial" >nul 2>&1 && set "PYEXE=py"
if not defined PYEXE python -c "import esptool, serial" >nul 2>&1 && set "PYEXE=python"
if not defined PYEXE goto :fehlt

:start
%PYEXE% "%~dp0tools\nrtool.py"
goto :ende

:fehlt
echo.
echo  Ohne Python + esptool geht es nicht. Python installieren (python.org, "Add to PATH"),
echo  dann in der Eingabeaufforderung:  py -m pip install esptool
pause

:ende
endlocal
