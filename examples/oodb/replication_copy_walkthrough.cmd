@echo off
rem replication_copy_walkthrough.cmd - 11/14/2026 (DataReel 7.4): the Windows replication example.
rem gxrepl (live replication) runs on Linux and other POSIX systems only. On Windows a copy of a
rem database folder is a plain file copy made while the writer is stopped, checked with digests:
rem   1. the primary's writer writes records with their two indexes and stops; robocopy copies the
rem      folder; the digests of the records, the B-tree and the vector index are the same on both;
rem   2. the writer writes more and stops; robocopy copies the changed files; the digests agree again;
rem   3. a simulated fault: the copy's two indexes lost; "replication_copy rebuild" makes them again
rem      from the copy's records, and the digests agree with the primary again.
rem vix_rebuild --digest checks the two vector indexes as well.
rem
rem   replication_copy_walkthrough.cmd BUILD\examples\Release BUILD\utils\Release [WORK_DIR]
rem
rem WORK_DIR (default: a new folder under %TEMP%) is removed at the end unless KEEP=1.
rem Exit 0 when every check passes.
setlocal EnableExtensions EnableDelayedExpansion
if "%~2"=="" (
  echo usage: replication_copy_walkthrough.cmd EXAMPLES_DIR UTILS_DIR [WORK_DIR]
  exit /b 64
)
set "APP=%~f1\replication_copy.exe"
set "VR=%~f2\vix_rebuild.exe"
if not exist "%APP%" ( echo FAILED: no %APP% & exit /b 1 )
if not exist "%VR%" ( echo FAILED: no %VR% & exit /b 1 )
if "%~3"=="" ( set "W=%TEMP%\repl_copy_%RANDOM%%RANDOM%" ) else ( set "W=%~f3" )
if exist "%W%\primary" rmdir /s /q "%W%\primary"
if exist "%W%\copy" rmdir /s /q "%W%\copy"
mkdir "%W%\primary" "%W%\copy" 2>nul
if not exist "%W%\primary" ( echo FAILED: cannot make %W% & exit /b 1 )

echo.
echo == 1. The primary writes, then stops; robocopy copies the folder
"%APP%" write "%W%\primary" --commits 200 --per 8 || goto fail
call :copy || goto fail
call :compare || goto fail

echo.
echo == 2. The writer writes more, then stops; robocopy copies the changed files
"%APP%" write "%W%\primary" --commits 100 --per 8 --seed 7 || goto fail
call :copy || goto fail
call :compare || goto fail

echo.
echo == 3. A simulated fault: the copy's two indexes lost
del /q "%W%\copy\by_name.btx" "%W%\copy\vectors.vix"
"%APP%" digest "%W%\copy" > "%W%\copy.digest" 2>&1
if not errorlevel 1 ( echo FAILED: the digest of the copy worked without its indexes & goto fail )
echo   digest of the copy: & type "%W%\copy.digest"
echo   the indexes made again from the copy's records:
"%APP%" rebuild "%W%\copy" || goto fail
call :compare || goto fail

echo.
echo OK: two folder copies and one index rebuild on the copy, the same records and indexes on both folders
if not "%KEEP%"=="1" rmdir /s /q "%W%"
exit /b 0

:fail
echo FAILED (work folder kept: %W%)
exit /b 1

rem Copy the stopped writer's folder: every database file, never its journal
:copy
robocopy "%W%\primary" "%W%\copy" /MIR /XF *.jnl /R:0 /W:0 /NFL /NDL /NJH /NJS /NP > "%W%\robocopy.log"
if errorlevel 8 ( echo   robocopy failed: & type "%W%\robocopy.log" & exit /b 1 )
echo   copied
exit /b 0

rem The digests of both folders, the same; vix_rebuild's digests of both vector indexes, the same
:compare
"%APP%" digest "%W%\primary" > "%W%\primary.digest" || ( type "%W%\primary.digest" & exit /b 1 )
"%APP%" digest "%W%\copy" > "%W%\copy.digest" || ( type "%W%\copy.digest" & exit /b 1 )
echo   primary: & type "%W%\primary.digest"
echo   copy: & type "%W%\copy.digest"
fc /b "%W%\primary.digest" "%W%\copy.digest" > nul || ( echo   the digests differ & exit /b 1 )
set "VP=" & set "VC="
for /f "usebackq tokens=1-3" %%a in (`""%VR%" --digest "%W%\primary\vectors.vix""`) do set "VP=%%a %%b %%c"
for /f "usebackq tokens=1-3" %%a in (`""%VR%" --digest "%W%\copy\vectors.vix""`) do set "VC=%%a %%b %%c"
if "!VP!"=="" ( echo   vix_rebuild --digest failed & exit /b 1 )
if not "!VP!"=="!VC!" ( echo   vix_rebuild digests differ: !VP! / !VC! & exit /b 1 )
echo   vix_rebuild --digest, both: !VP!
echo   the same on both folders
exit /b 0
