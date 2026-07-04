# win-wallpaper-engine

Windows-native Wallpaper Engine `.pkg` renderer

Direct3D 11 硬件加速 + FFmpeg 视频解码，可在 Windows 上渲染 Wallpaper Engine 的 `.pkg` 文件。

> 参考：原版 [open-wallpaper-engine](https://github.com/waywallen/open-wallpaper-engine)（Linux）和 [RePKG](https://github.com/notscuffed/repkg)（C# PKG/TEX 工具）

## 功能

- 加载 Wallpaper Engine `scene.pkg` 文件（PKGV 格式）
- 解析 `scene.json` 场景描述
- .tex 纹理解码（TEXV 格式，LZ4 解压，支持 PNG/RGBA8/DXT5）
- FFmpeg 7.1 视频解码（H.264/MP4）
- Direct3D 11 渲染 + HLSL 运行时编译
- 各向异性过滤 16x
- 键盘切换纹理（← →）/ ESC 退出

## 使用

```bat
win-wallpaper-engine.exe <scene.pkg> [--width N] [--height N]
```

| 按键 | 功能 |
|------|------|
| ← → | 切换显示纹理 |
| Esc | 退出 |

## 系统要求

- Windows 10 x64 或更高
- Direct3D 11 兼容显卡
- Wallpaper Engine `.pkg` 文件

## 本地编译

要求：Visual Studio 2022 Build Tools / CMake 3.20+ / Ninja

```bat
call "D:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

或直接运行 `build_manual.bat`

## 项目结构

```
src/
  main.cpp              # 入口 + 流程控制
  PkgFs.h/.cpp          # .pkg 文件读取（PKGV 格式）
  SceneDocument.h/.cpp  # scene.json 解析
  TexParser.h/.cpp      # .tex 纹理解码（stb_image + LZ4）
  Renderer.h/.cpp       # D3D11 渲染器
  WallpaperWindow.h/.cpp # Win32 窗口
  VideoDecoder.h/.cpp   # FFmpeg 视频解码
shaders/
  textured_vs.hlsl      # 顶点着色器
  textured_ps.hlsl      # 像素着色器
third_party/
  ffmpeg/               # FFmpeg 7.1 headers + libs + DLLs
  stb_image.h/.c        # 图片解码
  lz4.h/.c              # LZ4 解压
  nlohmann/json.hpp     # JSON 解析
```

## 技术细节

### .pkg 格式
- 版本戳：变长 `PKGV` 前缀字符串
- 目录表：int32 条目数 + 变长条目（含相对偏移）
- 无容器级压缩

### .tex 格式
- 三组 9 字节版本戳（TEXV / TEXI / TEXB）
- 格式字段：RGBA8 / DXT5 / 等
- Mipmap 层级，LZ4 压缩（texb >= 2）
- 视频检测：flags bit 5 + ftyp 魔数

## License

GPL-2.0
