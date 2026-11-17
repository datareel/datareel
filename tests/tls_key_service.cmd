@echo off
rem tls_key_service.cmd - 11/17/2026 (7.4): tls_key_test run by the service manager under LocalService (no user
rem signed in, no profile of the person running the tests). Needs an administrator command prompt. The service is
rem made for this run only (demand start, never automatic) and deleted at the end, also after a failure.
rem
rem   tls_key_service.cmd TLS_KEY_TEST_EXE PKI_DIR PORT WORK_DIR
rem
rem WORK_DIR (made here, e.g. C:\ProgramData\dr74_items_svc) gets the program, the certificates and the log, readable
rem by LocalService. Prints the log; exit 0 when every check passed.
setlocal
set SVC=DataReelTestSvc
set EXE=%~1
set PKI=%~2
set PORT=%~3
set WD=%~4
if "%WD%"=="" (echo usage: tls_key_service.cmd TLS_KEY_TEST_EXE PKI_DIR PORT WORK_DIR & exit /b 64)
sc query %SVC% > nul 2>&1 && (echo FAIL a service named %SVC% is already there: not touched & exit /b 1)
if not exist "%WD%" mkdir "%WD%"
mkdir "%WD%\pki" 2> nul
copy /y "%EXE%" "%WD%\tls_key_test.exe" > nul
copy /y "%PKI%\ca.pem" "%WD%\pki\" > nul
copy /y "%PKI%\copy.pem" "%WD%\pki\" > nul
copy /y "%PKI%\copy.key" "%WD%\pki\" > nul
copy /y "%PKI%\primary.pem" "%WD%\pki\" > nul
copy /y "%PKI%\primary.key" "%WD%\pki\" > nul
icacls "%WD%" /grant "NT AUTHORITY\LOCAL SERVICE:(OI)(CI)M" > nul
if exist "%WD%\svc.log" del "%WD%\svc.log"
sc create %SVC% binPath= "\"%WD%\tls_key_test.exe\" \"%WD%\pki\" %PORT% --log \"%WD%\svc.log\" --service %SVC%" start= demand obj= "NT AUTHORITY\LocalService" > nul
if errorlevel 1 (echo FAIL sc create & exit /b 1)
sc start %SVC% > nul
set RC=1
for /l %%i in (1,1,120) do (
  sc query %SVC% | find "STOPPED" > nul && goto :stopped
  ping -n 2 127.0.0.1 > nul
)
echo FAIL the service did not stop in 120 s: stopping it
sc stop %SVC% > nul
ping -n 4 127.0.0.1 > nul
:stopped
sc delete %SVC% > nul
ping -n 2 127.0.0.1 > nul
sc query %SVC% > nul 2>&1 && (echo FAIL %SVC% is still there after sc delete) || (echo service %SVC% deleted)
if exist "%WD%\svc.log" (type "%WD%\svc.log") else (echo FAIL no log: the service did not run the checks)
findstr /r /c:" 0 failed" "%WD%\svc.log" > nul 2>&1 && findstr /c:"running as a service" "%WD%\svc.log" > nul 2>&1 && set RC=0
exit /b %RC%
