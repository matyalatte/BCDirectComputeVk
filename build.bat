@echo off

REM Builds example-app with CMake and Visual Studio.

set VS_VERSION=Visual Studio 17 2022

mkdir %~dp0\build
@pushd %~dp0\build

cmake -G "%VS_VERSION%"^
 -D CMAKE_CONFIGURATION_TYPES=Debug^
 -D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded^
 ../

cmake --build . --config Debug
copy Debug\example-app.exe ..\
@popd

pause
