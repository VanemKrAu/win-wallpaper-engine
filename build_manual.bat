@echo off
setlocal enabledelayedexpansion
set MSVC=D:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207
set SDK=C:\Program Files (x86)\Windows Kits\10
set SVR=10.0.18362.0
set SRC=E:\WorkSpace\win-wallpaper-engine\src
set OUT=E:\WorkSpace\win-wallpaper-engine\build-manual
set TP=E:\WorkSpace\win-wallpaper-engine\third_party
set FFMPEG=%TP%\ffmpeg
set PATH=%MSVC%\bin\Hostx64\x64;%SDK%\bin\%SVR%\x64;%PATH%
set INCLUDE=%MSVC%\include;%SDK%\include\%SVR%\ucrt;%SDK%\include\%SVR%\um;%SDK%\include\%SVR%\shared;%TP%;%FFMPEG%\include
set LIB=%MSVC%\lib\x64;%SDK%\lib\%SVR%\ucrt\x64;%SDK%\lib\%SVR%\um\x64;%FFMPEG%
if not exist "%OUT%" mkdir "%OUT%"
cd /d "%OUT%"
set CF=/nologo /EHsc /std:c++20 /DUNICODE /D_UNICODE /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /I"%SRC%" /I"%TP%" /I"%TP%\nlohmann" /I"%FFMPEG%\include"
echo COMPILING
cl %CF% /c "%TP%\lz4.c" || exit /b 1
echo lz4 OK
cl %CF% /c "%TP%\stb_image.c" || exit /b 1
echo stb_image OK
cl %CF% /c "%SRC%\PkgFs.cpp" || exit /b 1
echo PkgFs OK
cl %CF% /c "%SRC%\SceneDocument.cpp" || exit /b 1
echo SceneDoc OK
cl %CF% /c "%SRC%\TexParser.cpp" || exit /b 1
echo TexParser OK
cl %CF% /c "%SRC%\WallpaperWindow.cpp" || exit /b 1
echo Win OK
cl %CF% /c "%SRC%\Renderer.cpp" || exit /b 1
echo Renderer OK
cl %CF% /c "%SRC%\VideoDecoder.cpp" || exit /b 1
echo VideoDecoder OK
cl %CF% /c "%SRC%\main.cpp" || exit /b 1
echo main OK
echo LINKING
link /nologo /OUT:"%OUT%\win-wallpaper-engine.exe" /MACHINE:X64 /SUBSYSTEM:WINDOWS "%OUT%\lz4.obj" "%OUT%\stb_image.obj" "%OUT%\PkgFs.obj" "%OUT%\SceneDocument.obj" "%OUT%\TexParser.obj" "%OUT%\WallpaperWindow.obj" "%OUT%\Renderer.obj" "%OUT%\VideoDecoder.obj" "%OUT%\main.obj" d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib ole32.lib shell32.lib "%FFMPEG%\avcodec-61.lib" "%FFMPEG%\avformat-61.lib" "%FFMPEG%\avutil-59.lib" "%FFMPEG%\swscale-8.lib"
if %ERRORLEVEL% NEQ 0 exit /b 1
copy "%FFMPEG%\bin\*.dll" "%OUT%" >NUL 2>&1
echo SUCCESS
dir "%OUT%\win-wallpaper-engine.exe"
