@echo off
:: run_db_tests.bat - 6.3: build and run every database example and the
:: journal tests with MSVC. Run from a VS 2026 developer prompt (for
:: example after env\msvc64_vs2026.bat), in the examples_database
:: folder, after building the library in winslib:
::   nmake -f project.mak all FINAL=1 64BITCFG=1 USESSL=0   (64-bit engine)
::   nmake -f project.mak all FINAL=1 64BITCFG=0 USESSL=0   (32-bit engine)
:: Pass the same 64BITCFG value to this script: run_db_tests.bat 1
:: Logs go to test_runs\<example>\.
setlocal
set CFG=%1
if "%CFG%"=="" set CFG=1
set PASS=0
set FAIL=0
set W=%~dp0test_runs
if not exist "%W%" mkdir "%W%"
for %%e in (btree btree_cached btree_node database database_cache defrag infohog pod simple journal) do call :build %%e
for %%e in (btree btree_cached btree_node database database_cache defrag infohog pod simple) do call :run %%e
call :journal 0
call :journal 1
echo %PASS% passed, %FAIL% failed (logs in %W%)
if %FAIL% NEQ 0 exit /b 1
exit /b 0

:build
pushd "%~dp0%1"
nmake -nologo -f msvc.mak FINAL=1 64BITCFG=%CFG% USESSL=0 > build.log 2>&1
if errorlevel 1 (echo FAIL build %1 & set /a FAIL+=1) else (echo PASS build %1 & set /a PASS+=1)
popd
exit /b 0

:run
if not exist "%W%\%1" mkdir "%W%\%1"
pushd "%W%\%1"
echo.| "%~dp0%1\testprog.exe" > run1.log 2>&1
if errorlevel 1 goto runfail
echo.| "%~dp0%1\testprog.exe" > run2.log 2>&1
if errorlevel 1 goto runfail
echo PASS %1 (create, then reopen)
set /a PASS+=1
popd
exit /b 0
:runfail
echo FAIL %1
set /a FAIL+=1
popd
exit /b 0

:journal
if not exist "%W%\journal_map%1" mkdir "%W%\journal_map%1"
pushd "%W%\journal_map%1"
"%~dp0journal\testprog.exe" 20 7 %1 > journal.log 2>&1
if errorlevel 1 (echo FAIL journal tests, memory-mapped reads %1 & set /a FAIL+=1) else (echo PASS journal tests, memory-mapped reads %1 & set /a PASS+=1)
popd
exit /b 0
