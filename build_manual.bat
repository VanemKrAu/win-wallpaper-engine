@echo off
setlocal enabledelayedexpansion
REM 手动编译 win-wallpaper-engine（绕过 CMake/Ninja 的路径转义问题）

set MSVC=D:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207
set SDK=C:\Program Files (x86)\Windows Kits\10
set SDKVER=10.0.18362.0
set SRC=E:\WorkSpace\win-wallpaper-engine\src
set OUT=E:\WorkSpace\win-wallpaper-engine\build-manual
set PROJ=E:\WorkSpace\win-wallpaper-engine

REM 使用短文件名（8.3格式）避免空格问题
for %%A in ("%MSVC%") do set MSVC_SHORT=%%~sA
for %%A in ("%SDK%") do set SDK_SHORT=%%~sA

echo MSVC short: %MSVC_SHORT%
echo SDK short: %SDK_SHORT%

REM 设置环境
set PATH=%MSVC%\bin\Hostx64\x64;%MSVC%\bin\Hostx64\x86;%SDK%\bin\%SDKVER%\x64;%PATH%
set INCLUDE=%MSVC%\include;%SDK%\include\%SDKVER%\ucrt;%SDK%\include\%SDKVER%\um;%SDK%\include\%SDKVER%\shared
set LIB=%MSVC%\lib\x64;%SDK%\lib\%SDKVER%\ucrt\x64;%SDK%\lib\%SDKVER%\um\x64

if not exist "%OUT%" mkdir "%OUT%"

cd /d "%OUT%"

echo === COMPILING ===

set CFLAGS=/nologo /EHsc /std:c++20 /DUNICODE /D_UNICODE /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /O2 /GL /I"%SRC%" /I"%PROJ%\third_party"

REM 逐个编译
cl %CFLAGS% /c "%SRC%\PkgFs.cpp"
if %ERRORLEVEL% NEQ 0 exit /b 1
echo PkgFs OK

cl %CFLAGS% /c "%SRC%\SceneDocument.cpp"
if %ERRORLEVEL% NEQ 0 exit /b 1
echo SceneDocument OK

cl %CFLAGS% /c "%SRC%\TexParser.cpp"
if %ERRORLEVEL% NEQ 0 exit /b 1
echo TexParser OK

cl %CFLAGS% /c "%SRC%\WallpaperWindow.cpp"
if %ERRORLEVEL% NEQ 0 exit /b 1
echo WallpaperWindow OK

cl %CFLAGS% /c "%SRC%\Renderer.cpp"
if %ERRORLEVEL% NEQ 0 exit /b 1
echo Renderer OK

cl %CFLAGS% /c "%SRC%\main.cpp"
if %ERRORLEVEL% NEQ 0 exit /b 1
echo main OK

echo === LINKING ===
link /nologo /OUT:"%OUT%\win-wallpaper-engine.exe" /LTCG /MACHINE:X64 /SUBSYSTEM:WINDOWS ^
  "%OUT%\PkgFs.obj" ^
  "%OUT%\SceneDocument.obj" ^
  "%OUT%\TexParser.obj" ^
  "%OUT%\WallpaperWindow.obj" ^
  "%OUT%\Renderer.obj" ^
  "%OUT%\main.obj" ^
  d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib ole32.lib

if %ERRORLEVEL% NEQ 0 (
  echo LINK FAILED
  exit /b 1
)

echo === SUCCESS ===
dir "%OUT%\win-wallpaper-engine.exe"
