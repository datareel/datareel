@echo off

:: Set the C/C++ env for VS 2026 for 32-bit x86 builds of legacy code
:: 10/24/2026: Use the vs2026_vars.bat file to find the VS 2026 install.
:: The default build in msvc.bat is now 64-bit x64.
call "%~dp0vs2026_vars.bat" x86
