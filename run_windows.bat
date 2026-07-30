@echo off
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "EXE=%~dp0build\windows\%CONFIG%\fastfreddy_testbed.exe"
if not exist "%EXE%" (
    echo Missing "%EXE%". Run build_windows.bat first.
    exit /b 1
)
pushd "%~dp0build\windows\%CONFIG%" >nul
fastfreddy_testbed.exe
set "ERR=%ERRORLEVEL%"
popd >nul
exit /b %ERR%
