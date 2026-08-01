@echo off
setlocal EnableExtensions

set "PROJECT_ROOT=%~dp0"
pushd "%PROJECT_ROOT%" >nul || exit /b 1

set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"
set "EXE=build\windows\%CONFIG%\sandhybrid.exe"

rem Always rebuild. Shader sources and generated SPIR-V can change while an old
rem executable still exists, and launching that stale build makes fixes appear
rem ineffective.
call build_windows.bat "%CONFIG%"
if errorlevel 1 (
    popd >nul
    exit /b 1
)

if not exist "%EXE%" (
    echo ERROR: Build completed without producing "%EXE%".
    popd >nul
    exit /b 1
)

echo [SandHybrid] Launching %EXE%...
"%EXE%"
set "RESULT=%ERRORLEVEL%"
echo [SandHybrid] Process exited with code %RESULT%.
popd >nul
exit /b %RESULT%
