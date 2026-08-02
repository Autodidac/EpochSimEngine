@echo off
setlocal
cd /d "%~dp0"

set "EXE="
for %%F in ("sandhybrid.exe" "bin\sandhybrid.exe" "Release\sandhybrid.exe" "build\Release\sandhybrid.exe" "build\sandhybrid.exe" "build\ci\Release\sandhybrid.exe" "build\ci\sandhybrid.exe") do (
    if exist "%%~F" set "EXE=%%~F"
)

if not defined EXE (
    echo SandHybrid executable not found.
    echo Build the Release target first, then run this file again.
    pause
    exit /b 1
)

echo Launching %EXE%
"%EXE%" %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" (
    echo SandHybrid exited with code %RESULT%.
    pause
)
exit /b %RESULT%
