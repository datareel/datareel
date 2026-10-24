@echo off

:: Set the C/C++ env for VS 2026 for 32-bit x86 builds
:: 10/24/2026: Use the vs2026_vars.bat file to find the VS 2026 install
call "%~dp0vs2026_vars.bat" x86
