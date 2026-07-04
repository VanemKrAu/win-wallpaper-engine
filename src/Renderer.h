#pragma once
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>

struct Vertex { float x, y, z, u, v; };
struct ConstantBuffer { float mvp[16]; };
struct PsConstantBuffer {
    float layerColor[4] = {1,1,1,1};
    float layerAlpha = 1.0f;
    float layerBrightness = 1.0f;
    float _padding[2] = {0,0};
};
struct Texture { ID3D11ShaderResourceView* srv = nullptr; int width = 0; int height = 0; };

struct Renderer {
    ID3D11Device*           device      = nullptr;
    ID3D11DeviceContext*    context     = nullptr;
    ID3D11VideoDevice*      vd          = nullptr;
    ID3D11VideoContext*     vc          = nullptr;
    ID3D11VideoProcessor*   vp          = nullptr;
    ID3D11VideoProcessorEnumerator* vpe = nullptr;
    IDXGISwapChain*         swapchain   = nullptr;
    ID3D11RenderTargetView* rtv         = nullptr;
    ID3D11RasterizerState*  rasterState = nullptr;
    ID3D11VertexShader*     vs          = nullptr;
    ID3D11PixelShader*      ps          = nullptr;
    ID3D11InputLayout*      layout      = nullptr;
    ID3D11SamplerState*     sampler     = nullptr;
    ID3D11Buffer*           vbo         = nullptr;
    ID3D11Buffer*           ibo         = nullptr;
    ID3D11Buffer*           cbuf        = nullptr;
    ID3D11Buffer*           cbuf_ps     = nullptr;
    std::vector<Texture>    textures;
    int width  = 3840, height = 2160;
    bool vpActive = false;

    bool Init(HWND hwnd, int w, int h);
    bool LoadShaders();
    bool CreateFullscreenQuad();
    bool CreateSampler();
    void Render(int texIndex = 0);
    void ProcessVideoFrame(const uint8_t* y, const uint8_t* uv, int tw, int th);
    void Resize(int w, int h);
    bool LoadTexture(const uint8_t* pixels, int w, int h, Texture* out);
    void Destroy();
};
