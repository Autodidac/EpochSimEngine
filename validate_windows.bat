@echo off
setlocal EnableExtensions

set "PROJECT_ROOT=%~dp0"
pushd "%PROJECT_ROOT%" >nul || exit /b 1
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"

call build_windows.bat "%CONFIG%"
if errorlevel 1 goto :fail

where python >nul 2>nul
if not errorlevel 1 python tools\validate_shader_contracts.py
if errorlevel 1 goto :fail

set "VALIDATION_DIR=build\windows-validation"
set "VCPKG_ROOT=C:\Users\iammi\source\repos\vcpkg"
cmake -S . -B "%VALIDATION_DIR%" ^
    -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE:FILEPATH="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DVCPKG_TARGET_TRIPLET:STRING=x64-windows ^
    -DVCPKG_HOST_TRIPLET:STRING=x64-windows ^
    -DBUILD_TESTING:BOOL=ON ^
    -DSANDHYBRID_WARNINGS_AS_ERRORS:BOOL=ON
if errorlevel 1 goto :fail

cmake --build "%VALIDATION_DIR%" --config "%CONFIG%" --parallel
if errorlevel 1 goto :fail
ctest --test-dir "%VALIDATION_DIR%" -C "%CONFIG%" --output-on-failure
if errorlevel 1 goto :fail

echo SandHybrid validation passed.
popd >nul
exit /b 0

:fail
popd >nul
exit /b 1
