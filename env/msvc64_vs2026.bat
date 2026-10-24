@echo off

:: Set the C/C++ env for VS 2026 for 64-bit x64 builds
:: 10/24/2026: Use the vs2026_vars.bat file to find the VS 2026 install.
:: Changed from vcvarsamd64_x86.bat, which builds 32-bit x86 programs
:: on a 64-bit host, to the x64 compiler that builds 64-bit programs.
call "%~dp0vs2026_vars.bat" x64
