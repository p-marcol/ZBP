@echo off
setlocal

rem Windows build script.
rem Not tested on Windows, so it may require small adjustments for a specific
rem compiler, generator, or CMake installation.

set "BUILD_DIR=%~1"
if "%BUILD_DIR%"=="" set "BUILD_DIR=build"

if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Debug"

echo Using build directory: %BUILD_DIR%
echo Using build type: %BUILD_TYPE%

cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE="%BUILD_TYPE%"
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config "%BUILD_TYPE%"
if errorlevel 1 exit /b %errorlevel%

