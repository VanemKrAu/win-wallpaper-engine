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

void PrintUsage() {
    printf("win-wallpaper-engine v0.1.0\n");
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
        if (arg == L"--width" && i + 1 < argc) {
            args.width = _wtoi(argv[++i]);
        } else if (arg == L"--height" && i + 1 < argc) {
            args.height = _wtoi(argv[++i]);
        } else if (arg[0] != L'-') {
            args.pkgPath = arg;
        }
    }
    return args;
}

// Check if a path looks like an image file
static bool IsImageFile(const std::string& name) {
    size_t dot = name.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext;
    for (size_t i = dot; i < name.size(); i++) ext.push_back((char)tolower(name[i]));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif";
}

// Scan pkg for image files
static std::vector<std::string> ScanImageFiles(const PkgFs& pkg) {
    std::vector<std::string> paths;
    for (size_t i = 0; i < pkg.Entries().size(); i++) {
        if (IsImageFile(pkg.Entries()[i].name)) {
            paths.push_back(pkg.Entries()[i].name);
        }
    }
    return paths;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    (void)nCmdShow;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    Args args = ParseArgs(argc, argv);
    LocalFree(argv);

    if (args.pkgPath.empty()) {
        PrintUsage();
        return 1;
    }

    wprintf(L"Loading: %s\n", args.pkgPath.c_str());

    PkgFs pkg;
    if (!pkg.Open(args.pkgPath.c_str())) {
        printf("ERROR: Failed to open .pkg file. Bad format or file not found.\n");
        return 1;
    }
    printf("  Version: %s\n", pkg.Version().c_str());
    printf("  Entries: %zu\n", pkg.Entries().size());

    // Parse scene.json
    SceneDocument scene;
    std::vector<uint8_t> sceneData = pkg.ReadFile("/scene.json");
    if (sceneData.empty()) {
        sceneData = pkg.ReadFile("/project.json");
    }
    if (!sceneData.empty()) {
        std::string jsonStr(sceneData.begin(), sceneData.end());
        printf("  scene.json: %zu bytes\n", sceneData.size());
        if (scene.Parse(jsonStr, pkg.Version())) {
            printf("  Scene objects: %zu, resolution: %dx%d\n",
                scene.objects.size(), scene.width, scene.height);
            if (scene.width > 0)  args.width  = scene.width;
            if (scene.height > 0) args.height = scene.height;
            for (size_t i = 0; i < scene.objects.size(); i++) {
                printf("    [%zu] type=%s name='%s' image='%s'\n",
                    i, scene.objects[i].type.c_str(),
                    scene.objects[i].name.c_str(),
                    scene.objects[i].imageRef.c_str());
            }
        } else {
            printf("  WARNING: Failed to parse scene.json\n");
        }
    } else {
        printf("  No scene.json/project.json found in pkg\n");
    }

    // Collect texture paths: from scene objects first, then scan pkg
    std::vector<std::string> texPaths = scene.AllTexturePaths();

    // Also try reading imageRef paths from the pkg directly
    // (scene.json paths may be /assets/textures/0.png while pkg stores textures/0.png)
    for (size_t i = 0; i < scene.objects.size(); i++) {
        if (scene.objects[i].imageRef.empty()) continue;
        std::string ref = scene.objects[i].imageRef;
        // Try different path variations
        std::string variants[] = {
            "/" + ref,
            "/assets/" + ref,
            "/assets/textures/" + ref,
        };
        for (int v = 0; v < 3; v++) {
            if (pkg.Find(variants[v])) {
                bool found = false;
                for (size_t t = 0; t < texPaths.size(); t++) {
                    if (texPaths[t] == variants[v]) { found = true; break; }
                }
                if (!found) texPaths.push_back(variants[v]);
            }
        }
    }

    // Fallback: scan all image files
    if (texPaths.empty()) {
        printf("  No textures from scene, scanning pkg for image files...\n");
        texPaths = ScanImageFiles(pkg);
    }

    printf("  Texture candidates: %zu\n", texPaths.size());
    for (size_t i = 0; i < texPaths.size(); i++) {
        printf("    - %s\n", texPaths[i].c_str());
    }

    // Create window
    wchar_t title[256];
    size_t lastSlash = args.pkgPath.find_last_of(L"/\\");
    std::wstring fname = (lastSlash != std::wstring::npos)
        ? args.pkgPath.substr(lastSlash + 1) : args.pkgPath;
    swprintf(title, 256, L"win-wallpaper-engine - %s", fname.c_str());

    WallpaperWindow win;
    if (!win.Create(title, args.width, args.height)) {
        printf("ERROR: Failed to create window\n");
        return 1;
    }

    // Init renderer
    Renderer renderer;
    if (!renderer.Init(win.Handle(), args.width, args.height)) {
        printf("ERROR: Failed to initialize D3D11 renderer\n");
        return 1;
    }
    printf("  D3D11 initialized: %dx%d\n", args.width, args.height);

    // Load textures
    int loadedCount = 0;
    for (size_t i = 0; i < texPaths.size(); i++) {
        std::vector<uint8_t> data = pkg.ReadFile(texPaths[i]);
        if (data.empty()) {
            printf("  SKIP (not found): %s\n", texPaths[i].c_str());
            continue;
        }

        DecodedImage img = DecodeImage(data);
        if (!img.Valid()) {
            printf("  SKIP (decode failed): %s (%zu bytes)\n", texPaths[i].c_str(), data.size());
            continue;
        }

        Texture tex;
        if (renderer.LoadTexture(img.pixels.data(), img.width, img.height, &tex)) {
            renderer.textures.push_back(tex);
            loadedCount++;
            printf("  LOADED: %s (%dx%d)\n", texPaths[i].c_str(), img.width, img.height);
        }
    }

    if (loadedCount == 0) {
        printf("WARNING: No textures loaded. Creating placeholder texture.\n");
        // Create a small placeholder texture so the renderer has something to show
        std::vector<uint8_t> placeholder(64 * 64 * 4, 0);
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int idx = (y * 64 + x) * 4;
                bool checker = ((x / 8) + (y / 8)) % 2 == 0;
                placeholder[idx+0] = checker ? 255 : 0;
                placeholder[idx+1] = 0;
                placeholder[idx+2] = checker ? 0 : 255;
                placeholder[idx+3] = 255;
            }
        }
        Texture tex;
        if (renderer.LoadTexture(placeholder.data(), 64, 64, &tex)) {
            renderer.textures.push_back(tex);
        }
    }

    printf("\nRendering: %d textures loaded. Use LEFT/RIGHT arrows to switch, ESC to exit.\n",
        loadedCount);

    // Message + render loop
    MSG msg = {};
    int texIndex = 0;
    bool active = true;

    while (active) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { active = false; break; }
            if (msg.message == WM_SIZE) {
                int nw = LOWORD(msg.lParam);
                int nh = HIWORD(msg.lParam);
                if (nw > 0 && nh > 0) renderer.Resize(nw, nh);
            }
            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_ESCAPE) { active = false; PostQuitMessage(0); break; }
                if (msg.wParam == VK_RIGHT && renderer.textures.size() > 1) {
                    texIndex = (texIndex + 1) % (int)renderer.textures.size();
                }
                if (msg.wParam == VK_LEFT && renderer.textures.size() > 1) {
                    texIndex = (texIndex - 1 + (int)renderer.textures.size()) % (int)renderer.textures.size();
                }
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!active) break;
        renderer.Render(texIndex);
    }

    renderer.Destroy();
    return 0;
}
