$CM = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $CM --build "C:\Users\tfroe\Documents\PROJECTS\AudioPluginSuite\build" --config Release --parallel 8
exit $LASTEXITCODE