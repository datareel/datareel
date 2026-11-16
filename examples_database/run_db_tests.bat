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
for %%e in (btree btree_cached btree_node damaged database database_cache defrag infohog layout pod simple journal) do call :build %%e
for %%e in (btree btree_cached btree_node damaged database database_cache defrag infohog layout pod simple) do call :run %%e
pushd "%~dp0..\examples_general\security_sweep"
nmake -nologo -f msvc.mak FINAL=1 64BITCFG=%CFG% USESSL=0 > build.log 2>&1
popd
if not exist "%W%\security_sweep" mkdir "%W%\security_sweep"
pushd "%W%\security_sweep"
"%~dp0..\examples_general\security_sweep\testprog.exe" > sweep.log 2>&1
if errorlevel 1 (echo FAIL security sweep & set /a FAIL+=1) else (echo PASS security sweep & set /a PASS+=1)
popd
pushd "%~dp0..\examples_general\support_security"
nmake -nologo -f msvc.mak FINAL=1 64BITCFG=%CFG% USESSL=0 > build.log 2>&1
popd
if not exist "%W%\support_security" mkdir "%W%\support_security"
pushd "%W%\support_security"
"%~dp0..\examples_general\support_security\testprog.exe" > support_security.log 2>&1
if errorlevel 1 (echo FAIL support_security & set /a FAIL+=1) else (echo PASS support_security & set /a PASS+=1)
popd
pushd "%~dp0..\examples_socket\comm_security"
nmake -nologo -f msvc.mak FINAL=1 64BITCFG=%CFG% USESSL=0 > build.log 2>&1
popd
if not exist "%W%\comm_security" mkdir "%W%\comm_security"
pushd "%W%\comm_security"
"%~dp0..\examples_socket\comm_security\testprog.exe" > comm_security.log 2>&1
if errorlevel 1 (echo FAIL comm_security & set /a FAIL+=1) else (echo PASS comm_security & set /a PASS+=1)
popd
:: 11/14/2026: 6.3.1 print drivers (the socket, SSL, stream and protocol
:: security tests use fork() and run on UNIX only)
pushd "%~dp0..\examples_general\print_security"
nmake -nologo -f msvc.mak FINAL=1 64BITCFG=%CFG% USESSL=0 > build.log 2>&1
popd
if not exist "%W%\print_security" mkdir "%W%\print_security"
pushd "%W%\print_security"
"%~dp0..\examples_general\print_security\testprog.exe" > print_security.log 2>&1
if errorlevel 1 (echo FAIL print_security & set /a FAIL+=1) else (echo PASS print_security & set /a PASS+=1)
popd
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
