cmake_minimum_required(VERSION 3.20)
project(win-wallpaper-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 第三方依赖 ─────────────────────────────────────────────────
# nlohmann_json (header-only)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# LZ4
FetchContent_Declare(
    lz4
    URL https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
)
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lz4)

# stb_image.h — 手写一个迷你 target
set(STB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb_image.c")
if(NOT EXISTS "${STB_SRC}")
    file(WRITE "${STB_SRC}"
        "#define STB_IMAGE_IMPLEMENTATION\n"
        "#include \"stb_image.h\"\n")
endif()
add_library(stb_image "${STB_SRC}")
target_include_directories(stb_image PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# ── Windows SDK — 由 vcvars64.bat 提供 ────────────────────────
# 在 vcvars64.bat 设置的环境下，d3d11/dxgi/d3dcompiler 可直接找到

# ── 主程序 ─────────────────────────────────────────────────────
add_executable(win-wallpaper-engine
    src/main.cpp
    src/PkgFs.cpp
    src/SceneDocument.cpp
    src/TexParser.cpp
    src/Renderer.cpp
    src/WallpaperWindow.cpp
)

target_include_directories(win-wallpaper-engine PRIVATE
    src
    ${nlohmann_json_SOURCE_DIR}/include
    ${lz4_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(win-wallpaper-engine
    PRIVATE
    nlohmann_json::nlohmann_json
    lz4
    stb_image
    d3d11
    d3dcompiler
    dxgi
    user32
    gdi32
)

target_compile_definitions(win-wallpaper-engine PRIVATE
    _UNICODE
    UNICODE
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)
