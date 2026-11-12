@echo off
rem NR Intercom - nur Konsole (C5), ohne zu flashen. Strg+F flasht trotzdem, Strg+Q beendet.
cd /d "%~dp0"
set "PYEXE="
py -c "import esptool, serial" >nul 2>&1 && set "PYEXE=py"
if not defined PYEXE python -c "import esptool, serial" >nul 2>&1 && set "PYEXE=python"
if not defined PYEXE (
    echo  Python mit esptool fehlt. Einmalig in der Eingabeaufforderung:  py -m pip install esptool
    pause
    exit /b 1
)
%PYEXE% "%~dp0..\tools\nrtool.py" c5 --monitor
