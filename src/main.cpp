#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cctype>
#include "PkgFs.h"
#include "SceneDocument.h"
#include "TexParser.h"
#include "Renderer.h"
#include "WallpaperWindow.h"
#include "VideoDecoder.h"

void PrintUsage() {
    printf("win-wallpaper-engine v0.1.0 with FFmpeg\n");
    printf("Usage: win-wallpaper-engine <scene.pkg> [--width N] [--height N]\n");
}

struct Args {
    std::wstring pkgPath;
    int width  = 1280;
    int height = 720;
};

Args ParseArgs(int argc, wchar_t* argv[]) {
    Args args;
    for (int i = 1; i < argc; i++) {
        std::wstring arg = argv[i];
        if (arg == L"--width" && i + 1 < argc) { args.width = _wtoi(argv[++i]); }
        else if (arg == L"--height" && i + 1 < argc) { args.height = _wtoi(argv[++i]); }
        else if (arg[0] != L'-') { args.pkgPath = arg; }
    }
    return args;
}

static bool IsImageFile(const std::string& name) {
    size_t dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext;
    for (size_t i = dot; i < name.size(); i++) ext.push_back((char)tolower(name[i]));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" || ext == ".tex";
}

static std::vector<std::string> ScanImageFiles(const PkgFs& pkg) {
    std::vector<std::string> paths;
    for (size_t i = 0; i < pkg.Entries().size(); i++) {
        if (IsImageFile(pkg.Entries()[i].name)) {
            paths.push_back(pkg.Entries()[i].name);
        }
    }
    return paths;
}

// Check if raw data starts with MP4 ftyp box
static bool IsVideoData(const uint8_t* data, size_t size) {
    if (size < 12) return false;
    return (std::memcmp(data + 4, "ftyp", 4) == 0);
}

// Texture that can be either static or video-updated
struct RenderTexture : Texture {
    VideoDecoder* decoder = nullptr;
    bool isVideo = false;
    int videoTexIdx = 0; // index into .tex data

    ~RenderTexture() { if (decoder) { decoder->Close(); delete decoder; } }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    (void)nCmdShow;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    Args args = ParseArgs(argc, argv);
    LocalFree(argv);

    if (args.pkgPath.empty()) { PrintUsage(); return 1; }

    wprintf(L"Loading: %s\n", args.pkgPath.c_str());

    PkgFs pkg;
    if (!pkg.Open(args.pkgPath.c_str())) {
        printf("ERROR: Failed to open .pkg file.\n");
        return 1;
    }
    printf("  Version: %s, Entries: %zu\n", pkg.Version().c_str(), pkg.Entries().size());

    // Parse scene.json
    SceneDocument scene;
    std::vector<uint8_t> sceneData = pkg.ReadFile("/scene.json");
    if (sceneData.empty()) sceneData = pkg.ReadFile("scene.json");
    if (sceneData.empty()) sceneData = pkg.ReadFile("/project.json");
    if (sceneData.empty()) sceneData = pkg.ReadFile("project.json");
    if (!sceneData.empty()) {
        std::string jsonStr(sceneData.begin(), sceneData.end());
        if (scene.Parse(jsonStr, pkg.Version())) {
            printf("  Scene objects: %zu, resolution: %dx%d\n", scene.objects.size(), scene.width, scene.height);
            if (scene.width > 0)  args.width  = scene.width;
            if (scene.height > 0) args.height = scene.height;
        }
    }

    // Find texture paths
    std::vector<std::string> texPaths = scene.AllTexturePaths();
    if (texPaths.empty()) {
        texPaths = ScanImageFiles(pkg);
    }
    printf("  Texture candidates: %zu\n", texPaths.size());

    // Create window
    wchar_t title[256];
    size_t lastSlash = args.pkgPath.find_last_of(L"/\\");
    std::wstring fname = (lastSlash != std::wstring::npos) ? args.pkgPath.substr(lastSlash + 1) : args.pkgPath;
    swprintf(title, 256, L"win-wallpaper-engine - %s", fname.c_str());

    WallpaperWindow win;
    if (!win.Create(title, args.width, args.height)) {
        printf("ERROR: Failed to create window\n");
        return 1;
    }

    Renderer renderer;
    if (!renderer.Init(win.Handle(), args.width, args.height)) {
        printf("ERROR: Failed to initialize D3D11 renderer\n");
        return 1;
    }
    printf("  D3D11 initialized: %dx%d\n", args.width, args.height);

    // Load textures: try static decode first, then video
    int loadedCount = 0;
    std::vector<RenderTexture*> renderTextures; // use RawTexture* to manage

    for (size_t i = 0; i < texPaths.size(); i++) {
        printf("  Loading: %s\n", texPaths[i].c_str());
        std::vector<uint8_t> data = pkg.ReadFile(texPaths[i]);
        if (data.empty()) { printf("    -> not found\n"); continue; }

        bool isTex = texPaths[i].size() > 4 &&
            (texPaths[i].substr(texPaths[i].size() - 4) == ".tex" ||
             texPaths[i].substr(texPaths[i].size() - 4) == ".TEX");

        DecodedImage img;
        if (isTex) img = DecodeTexFile(data);
        if (!img.Valid()) img = DecodeImage(data);

        RenderTexture* rt = new RenderTexture();

        if (img.Valid() && img.width > 0) {
            // Static image loaded
            if (renderer.LoadTexture(img.pixels.data(), img.width, img.height, rt)) {
                printf("    -> static image: %dx%d\n", img.width, img.height);
                renderer.textures.push_back(*rt);
                renderTextures.push_back(rt);
                loadedCount++;
                continue;
            }
        }

        // Check if it's a video texture (MP4 inside .tex)
        if (isTex && data.size() > 100) {
            // .tex video data starts after header + mip fields (~87 bytes for texb=3)
            // Just check at expected offsets instead of scanning 97MB
            size_t checkOffsets[] = {63, 67, 71, 75, 87, 91, 100, 120, 150, 200};
            for (int ci = 0; ci < 10; ci++) {
                size_t off = checkOffsets[ci];
                if (off >= data.size()) break;
                if (IsVideoData(data.data() + off, data.size() - off)) {
                    VideoDecoder* dec = new VideoDecoder();
                    if (dec->Open(data.data() + off, data.size() - off)) {
                        printf("    -> video: %dx%d, %.1f fps, %.1fs\n",
                            dec->Width(), dec->Height(), dec->FrameRate(), dec->Duration());
                        // Decode first frame immediately (it's fast)
                        VideoFrame firstFrame;
                        if (dec->DecodeNextFrame(firstFrame) && firstFrame.valid) {
                            printf("    -> first frame decoded: %dx%d\n", firstFrame.width, firstFrame.height);
                            if (renderer.LoadTexture(firstFrame.pixels.data(), firstFrame.width, firstFrame.height, rt)) {
                                rt->isVideo = true;
                                rt->decoder = dec;
                                rt->videoTexIdx = (int)off;
                                renderer.textures.push_back(*rt);
                                renderTextures.push_back(rt);
                                loadedCount++;
                                printf("    -> video texture loaded\n");
                                goto next_texture;
                            }
                        }
                        dec->Close();
                        delete dec;
                        dec->Close();
                        delete dec;
                    } else {
                        delete dec;
                    }
                    break;
                }
            }
        }

        // Skip compressed or failed
        if (img.width <= 0) printf("    -> compressed/skipped\n");
        else printf("    -> decode failed\n");
        delete rt;
next_texture:;
    }

    // Placeholder
    if (loadedCount == 0) {
        printf("WARNING: No textures loaded. Creating placeholder.\n");
        std::vector<uint8_t> placeholder(64 * 64 * 4, 0);
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++) {
                int idx = (y * 64 + x) * 4;
                bool checker = ((x / 8) + (y / 8)) % 2 == 0;
                placeholder[idx+0] = checker ? 255 : 0;
                placeholder[idx+1] = 0;
                placeholder[idx+2] = checker ? 0 : 255;
                placeholder[idx+3] = 255;
            }
        RenderTexture* rt = new RenderTexture();
        if (renderer.LoadTexture(placeholder.data(), 64, 64, rt)) {
            renderer.textures.push_back(*rt);
            renderTextures.push_back(rt);
        }
    }

    printf("\nRendering: %d textures loaded. LEFT/RIGHT to switch, ESC to exit.\n", loadedCount);

    // Render loop with video playback
    MSG msg = {};
    int texIndex = 0;
    bool active = true;
    DWORD lastFrameTime = GetTickCount();

    while (active) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { active = false; break; }
            if (msg.message == WM_SIZE) {
                int nw = LOWORD(msg.lParam), nh = HIWORD(msg.lParam);
                if (nw > 0 && nh > 0) renderer.Resize(nw, nh);
            }
            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_ESCAPE) { active = false; PostQuitMessage(0); break; }
                if (msg.wParam == VK_RIGHT && renderer.textures.size() > 1)
                    texIndex = (texIndex + 1) % (int)renderer.textures.size();
                if (msg.wParam == VK_LEFT && renderer.textures.size() > 1)
                    texIndex = (texIndex - 1 + (int)renderer.textures.size()) % (int)renderer.textures.size();
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!active) break;

        // Update video frame if current texture is a video (max ~30fps)
        DWORD now = GetTickCount();
        if (texIndex >= 0 && texIndex < (int)renderTextures.size() && now - lastFrameTime > 30) {
            RenderTexture* rt = renderTextures[texIndex];
            if (rt->isVideo && rt->decoder) {
                // Lazy-decode first frame
                if (rt->width == 64 && rt->height == 64) {
                    VideoFrame firstFrame;
                    if (rt->decoder->DecodeNextFrame(firstFrame) && firstFrame.valid) {
                        ID3D11ShaderResourceView* oldSrv = rt->srv;
                        if (renderer.LoadTexture(firstFrame.pixels.data(), firstFrame.width, firstFrame.height, rt)) {
                            if (oldSrv) oldSrv->Release();
                            // Sync the renderer's copy
                            renderer.textures[texIndex].srv = rt->srv;
                            renderer.textures[texIndex].width = rt->width;
                            renderer.textures[texIndex].height = rt->height;
                        }
                    }
                } else {
                    VideoFrame frame;
                    if (rt->decoder->DecodeNextFrame(frame) && frame.valid) {
                        ID3D11ShaderResourceView* oldSrv = rt->srv;
                        if (renderer.LoadTexture(frame.pixels.data(), frame.width, frame.height, rt)) {
                            if (oldSrv) oldSrv->Release();
                            // Sync the renderer's copy
                            renderer.textures[texIndex].srv = rt->srv;
                            renderer.textures[texIndex].width = rt->width;
                            renderer.textures[texIndex].height = rt->height;
                        }
                    } else {
                        rt->decoder->SeekToStart();
                    }
                }
                lastFrameTime = now;
            }
        }

        renderer.Render(texIndex);
    }

    renderer.Destroy();
    for (size_t i = 0; i < renderTextures.size(); i++) {
        delete renderTextures[i];
    }
    return 0;
}
