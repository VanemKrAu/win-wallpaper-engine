# win-wallpaper-engine

**Windows-native Wallpaper Engine `.pkg` 文件渲染器**

一个用 Direct3D 11 从零构建的轻量级 Wallpaper Engine 场景渲染器，可以在 Windows 上直接加载并显示 Wallpaper Engine 的 `.pkg` 文件。

> 本项目是 [open-wallpaper-engine](https://github.com/waywallen/open-wallpaper-engine) 的 Windows 简化移植版。原版项目仅支持 Linux，此版本使用 Windows 原生图形 API（D3D11）实现。

## 功能

- 加载 Wallpaper Engine `scene.pkg` 文件
- 解析 `scene.json` / `project.json` 场景描述
- 提取并解码内嵌的 PNG/JPEG 纹理（stb_image）
- Direct3D 11 硬件加速渲染
- 运行时编译 HLSL 着色器（无需 fxc / DirectX SDK）
- 键盘切换纹理（← → 键）
- 响应式窗口大小调整

## 系统要求

- **Windows 10 或更高版本**（x64）
- **Direct3D 11 兼容显卡**
- **Wallpaper Engine `.pkg` 文件**（来自 Steam Workshop）

## 快速开始

### 下载

从 [Releases](https://github.com/VanemKrAu/win-wallpaper-engine/releases) 下载最新版 `win-wallpaper-engine.exe`。

### 使用

```bat
win-wallpaper-engine.exe <scene.pkg> [--width N] [--height N]
```

示例：

```bat
win-wallpaper-engine.exe "C:\Steam\steamapps\workshop\content\431960\123456789\scene.pkg"
win-wallpaper-engine.exe scene.pkg --width 1920 --height 1080
```

### 控制

| 按键 | 功能 |
|------|------|
| `←` `→` | 切换显示纹理 |
| `Esc` | 退出 |

## 从源码构建

### 前置要求

- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) 或 Visual Studio 2022
- [CMake](https://cmake.org/download/) 3.20+
- [Ninja](https://ninja-build.org/)（可选，用于 CMake+Ninja 构建）

### 构建步骤

**方法 1：手动编译（推荐）**

```bat
# 在 Visual Studio 开发人员命令提示符中运行
cd /d E:\WorkSpace\win-wallpaper-engine
call build_manual.bat
```

**方法 2：CMake + Ninja**

```bat
# 在 Visual Studio 开发人员命令提示符中运行
cd /d E:\WorkSpace\win-wallpaper-engine
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

编译产物位于 `build-manual/` 或 `build/` 目录下。

## 项目结构

```
win-wallpaper-engine/
├── src/
│   ├── main.cpp              # 程序入口：CLI 解析 + 流程控制
│   ├── PkgFs.h/.cpp          # .pkg 文件读取（PKGV 格式）
│   ├── SceneDocument.h/.cpp  # scene.json 场景描述解析
│   ├── TexParser.h/.cpp      # 纹理解码（stb_image）
│   ├── Renderer.h/.cpp       # D3D11 渲染器
│   └── WallpaperWindow.h/.cpp # Win32 窗口管理
├── shaders/
│   ├── textured_vs.hlsl      # 顶点着色器（HLSL）
│   └── textured_ps.hlsl      # 像素着色器（HLSL）
├── third_party/              # 第三方依赖
│   ├── stb_image.h/.c        # 图片解码
│   ├── lz4.h/.c              # LZ4 解压（.tex 文件用）
│   └── nlohmann/json.hpp     # JSON 解析
├── CMakeLists.txt            # CMake 构建配置
├── build_manual.bat          # 手动编译脚本
└── build_win.bat             # CMake 构建脚本
```

## 技术细节

### .pkg 文件格式

Wallpaper Engine 的 `.pkg` 文件是一种简单的归档格式：

- **版本戳**：变长字符串，前缀为 `PKGV` + 版本号（如 `PKGV0023`）
- **目录表**：int32 条目数 +（int32 路径长度 + 路径字符串 + int32 相对偏移 + int32 长度）× N
- **文件数据**：目录表后的原始文件拼接
- **无容器级压缩**：LZ4 压缩仅用于 `.tex` 纹理文件的 mipmap 层

### 渲染架构

着色器在运行时通过 `D3DCompile` 编译，无需预装 DirectX SDK。程序启动时：

1. 加载 `.pkg` → 解析目录表
2. 读取 `scene.json` → 提取场景对象和纹理引用
3. 扫描并解码纹理 → 上传到 GPU
4. 进入渲染循环：全屏四边形 → 纹理采样 → 呈现

## 许可证

本项目基于 GPL-2.0 许可证开源。详见 [LICENSE](LICENSE) 文件。

## 致谢

- [open-wallpaper-engine](https://github.com/waywallen/open-wallpaper-engine) — 原版 Linux 渲染器，提供了 .pkg 格式的参考实现
- [stb](https://github.com/nothings/stb) — 单头文件图像库
- [nlohmann/json](https://github.com/nlohmann/json) — JSON 解析库
- [lz4](https://github.com/lz4/lz4) — 快速压缩算法
