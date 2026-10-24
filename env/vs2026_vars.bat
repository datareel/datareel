@echo off
:: --------------------------------------------------------------- 
:: BAT file Contents: Set the C/C++ build ENV for Visual Studio 2026
:: Produced By: DataReel Software Development Team
:: File Creation Date: 10/24/2026
:: Date Last Modified: 10/24/2026
:: --------------------------------------------------------------- 
:: Usage: call env\vs2026_vars.bat [x64 | x86 | arm64 | amd64_x86]
:: The default is x64.
::
:: Uses vswhere.exe to find the VS 2026 (version 18) install, so this
:: works with the Community, Professional, Enterprise, Build Tools,
:: and Insiders editions. Set DR_VSDIR to your VS install directory
:: before calling this file to skip the search.
:: --------------------------------------------------------------- 
set "DR_VSARCH=%~1"
if "%DR_VSARCH%"=="" set "DR_VSARCH=x64"

set "DR_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined DR_VSDIR if exist "%DR_VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%DR_VSWHERE%" -latest -prerelease -products * -version 18.0 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DR_VSDIR=%%i"

:: Fall back to the default install directories
if not defined DR_VSDIR if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" set "DR_VSDIR=%ProgramFiles%\Microsoft Visual Studio\18\Community"
if not defined DR_VSDIR if exist "%ProgramFiles%\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "DR_VSDIR=%ProgramFiles%\Microsoft Visual Studio\18\Professional"
if not defined DR_VSDIR if exist "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "DR_VSDIR=%ProgramFiles%\Microsoft Visual Studio\18\Enterprise"
if not defined DR_VSDIR if exist "%ProgramFiles%\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" set "DR_VSDIR=%ProgramFiles%\Microsoft Visual Studio\18\Insiders"
if not defined DR_VSDIR if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "DR_VSDIR=%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools"

if not defined DR_VSDIR echo ERROR: Could not find a Visual Studio 2026 install with the C++ tools
if not defined DR_VSDIR exit /b 1

echo Using Visual Studio in %DR_VSDIR%
call "%DR_VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" %DR_VSARCH%
