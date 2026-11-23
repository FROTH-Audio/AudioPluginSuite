@echo off
rem Run the full CTest suite (Release, bank build = 28 entries).
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build" -C Release --output-on-failure
