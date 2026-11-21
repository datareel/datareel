@echo off
rem build_wheel_windows.bat - DataReel 7.5: builds the datareel wheel on Windows x64.
rem
rem NOT TESTED and NOT DISTRIBUTED: DataReel ships Linux wheels only. This script is here so a Windows wheel can be
rem built if one is ever needed (see python/docs/building.md, "Windows").
rem python\CMakeLists.txt builds DataReel with OpenSSL (DATAREEL_SSL OPENSSL), not the Windows default Schannel:
rem replication and gxStream use PEM certificates.
rem
rem   python\build_wheel_windows.bat [OPENSSL_DIR] [PYTHON]       from the repository top
rem
rem Needs, in a "x64 Native Tools Command Prompt for VS" (Visual Studio 2022 or later, C++ workload):
rem   - CMake 3.16 or later on PATH (the one Visual Studio installs is fine)
rem   - CPython 3.12 x64 with pip, setuptools and wheel (py -3.12 -m pip install setuptools wheel)
rem   - OpenSSL 3.x for x64 (headers, libs and DLLs), at OPENSSL_DIR (default C:\Program Files\OpenSSL-Win64)
rem The wheel goes to dist\. The extension links OpenSSL's DLLs (libssl-3-x64.dll, libcrypto-3-x64.dll): they are
rem copied next to _datareel.pyd in the wheel, so it runs without OpenSSL on PATH.
setlocal
set OPENSSL_DIR=%~1
if "%OPENSSL_DIR%"=="" set OPENSSL_DIR=C:\Program Files\OpenSSL-Win64
set PY=%~2
if "%PY%"=="" set PY=py -3.12
if not exist "%OPENSSL_DIR%\include\openssl\ssl.h" (echo no OpenSSL headers in %OPENSSL_DIR%\include & exit /b 2)
if not exist "include\gxobjrepl.h" (echo run this from the DataReel repository top & exit /b 2)
where cmake > nul 2>&1 || (echo cmake is not on PATH: use a Visual Studio x64 command prompt & exit /b 2)
rem FindOpenSSL reads OPENSSL_ROOT_DIR from the environment
set OPENSSL_ROOT_DIR=%OPENSSL_DIR%
set DATAREEL_JOBS=2
if exist dist rmdir /s /q dist
%PY% -m pip wheel python --no-build-isolation --no-deps -w dist || (echo the wheel did not build & exit /b 1)
rem the OpenSSL DLLs into the wheel, next to the extension (a wheel is a zip file)
for %%W in (dist\datareel-*.whl) do (
  if exist _whl rmdir /s /q _whl
  mkdir _whl\datareel
  copy /y "%OPENSSL_DIR%\bin\libssl-3-x64.dll" _whl\datareel\ > nul
  copy /y "%OPENSSL_DIR%\bin\libcrypto-3-x64.dll" _whl\datareel\ > nul
  pushd _whl
  tar -a -u -f "..\%%W" datareel\libssl-3-x64.dll datareel\libcrypto-3-x64.dll 2> nul || echo add the two OpenSSL DLLs to the wheel's datareel\ folder by hand
  popd
  rmdir /s /q _whl
  echo built %%W
)
echo Check it: %PY% -m pip install --no-index dist\datareel-*.whl, then %PY% -c "import datareel; print(datareel.library_version())"
endlocal
