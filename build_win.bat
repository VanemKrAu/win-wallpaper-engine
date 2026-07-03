@echo off
setlocal enabledelayedexpansion
call "D:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
set "PATH=D:\Program Files\CMake\bin;D:\Ninja;%PATH%"

set "PROJ=E:\WorkSpace\win-wallpaper-engine"
cd /d "%PROJ%"

REM 删除旧的缓存文件
if exist "third_party\stb_image.c" del "third_party\stb_image.c"

REM 清理并重新配置
if exist "build-windows" rmdir /s /q "build-windows"
mkdir "build-windows"
cd /d "%PROJ%\build-windows"

echo === CMAKE CONFIGURE ===
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
  echo CMAKE CONFIGURE FAILED
  exit /b 1
)

echo === CMAKE BUILD ===
ninja -j4
if %ERRORLEVEL% NEQ 0 (
  echo BUILD FAILED
  exit /b 2
)

echo === SUCCESS ===
dir *.exe
