@echo off
rem ==============================================================================
rem AudioPluginSuite Build Batch - Plan 17
rem Removes existing build directory, configures, and builds with plugins + preset bank
rem ==============================================================================

rem --- Remove existing build directory (if present) ---
if exist build (
    rmdir /S /Q build
    echo Removed existing build directory.
)

rem --- Configure CMake with plugins and preset bank enabled ---
echo Configuring CMake...
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S . -B build -A x64 -DSUITE_BUILD_PLUGINS=ON -DSUITE_PRESET_BANK=ON

rem --- Build in Release mode ---
echo Building Release...
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release --parallel 8

echo Build complete.
pause