@echo off
setlocal EnableExtensions

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

set "ROOT=%~dp0"
pushd "%ROOT%" >nul

where git >nul 2>nul || (
    echo Git is required to bootstrap vcpkg and EpochGui.
    popd >nul
    exit /b 1
)
where cmake >nul 2>nul || (
    echo CMake 3.28 or newer is required.
    popd >nul
    exit /b 1
)

if not defined VCPKG_ROOT set "VCPKG_ROOT=%ROOT%.deps\vcpkg"
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo Bootstrapping vcpkg into "%VCPKG_ROOT%"...
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%" || goto :fail
    git -C "%VCPKG_ROOT%" checkout 04a735608afac5844e86fc91d6ba2112cac613c1 || goto :fail
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics || goto :fail
)

cmake --preset windows-msvc || goto :fail
cmake --build --preset windows-release --parallel || goto :fail
ctest --preset windows-release || goto :fail

echo.
echo Built: build\windows\%CONFIG%\fastfreddy_testbed.exe
popd >nul
exit /b 0

:fail
set "ERR=%ERRORLEVEL%"
if "%ERR%"=="0" set "ERR=1"
popd >nul
exit /b %ERR%
