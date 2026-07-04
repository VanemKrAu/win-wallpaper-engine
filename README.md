# win-wallpaper-engine

Windows 原生 Wallpaper Engine `.pkg` 渲染器

Direct3D 11 + FFmpeg 视频解码，可渲染 WE 的 `.pkg` 壁纸文件。

## 功能

- 加载 Wallpaper Engine `scene.pkg`（PKGV 格式）
- FFmpeg 7.1 视频解码（H.264/MP4，60fps）
- Lanczos 高质量 YUV→RGB
- 各向异性 16x 过滤
- 自动检测屏幕分辨率
- 窄高比填满（Fill 模式，不拉伸）
- .tex 纹理解码（TEXV 格式 + LZ4）
- D3D11 Video Processor 硬件管线（预留）
- 文件日志（`.log` 同目录）

## 使用

```bat
win-wallpaper-engine.exe <scene.pkg> [--width N] [--height N]
```

| 按键 | 功能 |
|------|------|
| ← → | 切换纹理 |
| Esc | 退出 |

## 编译

需要 Visual Studio 2022 Build Tools + CMake 3.20+

```bat
call vcvars64.bat
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

或运行 `build_manual.bat`

## 项目结构

```
src/
  main.cpp              # 入口 + 流程
  PkgFs.h/.cpp          # .pkg 读取 (PKGV)
  SceneDocument.h/.cpp  # scene.json 解析
  TexParser.h/.cpp      # .tex 纹理 (stb_image+LZ4)
  Renderer.h/.cpp       # D3D11 渲染 (含 VP)
  WallpaperWindow.h/.cpp # Win32 窗口
  VideoDecoder.h/.cpp   # FFmpeg 解码 (NV12+RGBA)
shaders/
  textured_vs.hlsl / textured_ps.hlsl
third_party/
  ffmpeg/  stb  lz4  nlohmann_json
```

## License

GPL-2.0
