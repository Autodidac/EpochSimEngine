@echo off
setlocal EnableExtensions

set "PROJECT_ROOT=%~dp0"
pushd "%PROJECT_ROOT%" >nul || exit /b 1

set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"

rem Keep the project on Adam's standalone vcpkg installation. Do not call
rem VsDevCmd.bat here: it can overwrite VCPKG_ROOT with Visual Studio's
rem private vcpkg copy.
if defined VCPKG_ROOT (
    set "SANDHYBRID_VCPKG_ROOT=%VCPKG_ROOT%"
) else (
    set "SANDHYBRID_VCPKG_ROOT=C:\Users\iammi\source\repos\vcpkg"
)
set "VCPKG_ROOT=%SANDHYBRID_VCPKG_ROOT%"

if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" (
    echo ERROR: vcpkg toolchain not found at "%VCPKG_ROOT%".
    popd >nul
    exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg.exe is missing.
    echo Run "%VCPKG_ROOT%\bootstrap-vcpkg.bat" first.
    popd >nul
    exit /b 1
)

where cmake >nul 2>nul || (
    echo ERROR: cmake is not on PATH.
    popd >nul
    exit /b 1
)

rem Verify that the requested Visual Studio generator is actually installed.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe was not found.
    echo Install Visual Studio 2022 with Desktop development with C++.
    popd >nul
    exit /b 1
)

set "VS_INSTALL="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL=%%I"
if not defined VS_INSTALL (
    echo ERROR: Visual Studio 2022 with the MSVC x64 toolchain was not found.
    popd >nul
    exit /b 1
)

set "BUILD_DIR=build\windows"

echo Using Visual Studio:
echo %VS_INSTALL%
echo Using vcpkg:
echo %VCPKG_ROOT%

rem The Visual Studio generator locates cl.exe and MSBuild itself. Ninja and
rem a Developer Command Prompt are intentionally not required.
cmake -S . -B "%BUILD_DIR%" ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET:STRING=x64-windows ^
    -DVCPKG_HOST_TRIPLET:STRING=x64-windows ^
    -DBUILD_TESTING:BOOL=OFF
if errorlevel 1 (
    popd >nul
    exit /b 1
)

rem Force regeneration of the movement shader. This prevents an extracted fix
rem from launching a stale move.comp.spv left in an existing build directory.
if exist "%BUILD_DIR%\generated\shaders\move.comp.spv" del /q "%BUILD_DIR%\generated\shaders\move.comp.spv"
if exist "%BUILD_DIR%\%CONFIG%\shaders\move.comp.spv" del /q "%BUILD_DIR%\%CONFIG%\shaders\move.comp.spv"

cmake --build "%BUILD_DIR%" --config "%CONFIG%" --parallel
if errorlevel 1 (
    popd >nul
    exit /b 1
)


echo Built: %CD%\%BUILD_DIR%\%CONFIG%\sandhybrid.exe
popd >nul
exit /b 0
