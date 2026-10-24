@echo off

:: Set the C/C++ env for VS 2026 for 64-bit x64 builds (the default)
:: 10/24/2026: The default build is now 64-bit x64. For 32-bit x86
:: builds of legacy code use msvc32_vs2026.bat.
call "%~dp0vs2026_vars.bat" x64
