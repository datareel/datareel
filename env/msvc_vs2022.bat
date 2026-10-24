@echo off

:: Set the C/C++ env for VS 2022 community edition
:: 10/24/2026: Set up 64-bit x64 builds by default. For 32-bit x86
:: builds remove the -arch and -host_arch options.
call  "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
