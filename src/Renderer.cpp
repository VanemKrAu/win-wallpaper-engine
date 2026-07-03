#include "Renderer.h"
#include <cassert>
#include <cstring>

static const char* g_vsSrc = R"(
struct VSInput {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
};
struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
cbuffer ConstantBuffer : register(b0) {
    float4x4 modelViewProj;
};
VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.pos = mul(float4(input.pos, 1.0f), modelViewProj);
    output.uv  = input.uv;
    return output;
}
)";

static const char* g_psSrc = R"(
Texture2D    diffuseTexture : register(t0);
SamplerState samplerState  : register(s0);
cbuffer LayerBuffer : register(b1) {
    float4 layerColor;
    float  layerAlpha;
    float  layerBrightness;
    float2 _padding;
};
struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
float4 PSMain(PSInput input) : SV_TARGET {
    float4 texel = diffuseTexture.Sample(samplerState, input.uv);
    float4 color = texel * layerColor;
    color.a *= layerAlpha;
    color.rgb *= layerBrightness;
    return color;
}
)";

static const Vertex g_quadVertices[] = {
    {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f},
    { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f},
    {-1.0f,  1.0f, 0.0f, 0.0f, 0.0f},
    { 1.0f,  1.0f, 0.0f, 1.0f, 0.0f},
};

static const uint16_t g_quadIndices[] = {0, 1, 2, 2, 1, 3};

bool Renderer::Init(HWND hwnd, int w, int h) {
    width  = w;
    height = h;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 2;
    scd.BufferDesc.Width  = w;
    scd.BufferDesc.Height = h;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count   = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed    = TRUE;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags       = 0;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        levels, 2, D3D11_SDK_VERSION,
        &scd, &swapchain, &device, nullptr, &context
    );
    if (FAILED(hr)) return false;

    // Compile shaders from source
    if (!LoadShaders()) return false;
    if (!CreateFullscreenQuad()) return false;
    if (!CreateSampler()) return false;

    Resize(w, h);
    return true;
}

bool Renderer::LoadShaders() {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* err    = nullptr;

    HRESULT hr = D3DCompile(g_vsSrc, strlen(g_vsSrc), "vs", nullptr, nullptr,
        "VSMain", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vsBlob, &err);
    if (FAILED(hr)) {
        if (err) { OutputDebugStringA((const char*)err->GetBufferPointer()); err->Release(); }
        return false;
    }

    hr = D3DCompile(g_psSrc, strlen(g_psSrc), "ps", nullptr, nullptr,
        "PSMain", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &psBlob, &err);
    if (FAILED(hr)) {
        if (err) { OutputDebugStringA((const char*)err->GetBufferPointer()); err->Release(); }
        vsBlob->Release();
        return false;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &vs);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC elemDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    device->CreateInputLayout(elemDesc, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout);

    vsBlob->Release();
    psBlob->Release();
    return true;
}

bool Renderer::CreateFullscreenQuad() {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = sizeof(g_quadVertices);
    bd.Usage          = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.StructureByteStride = sizeof(Vertex);

    D3D11_SUBRESOURCE_DATA sd = {g_quadVertices, 0, 0};
    HRESULT hr = device->CreateBuffer(&bd, &sd, &vbo);
    if (FAILED(hr)) return false;

    bd.ByteWidth      = sizeof(g_quadIndices);
    bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    bd.StructureByteStride = sizeof(uint16_t);
    sd.pSysMem = g_quadIndices;
    hr = device->CreateBuffer(&bd, &sd, &ibo);
    if (FAILED(hr)) return false;

    bd = {};
    bd.ByteWidth      = sizeof(ConstantBuffer);
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;

    ConstantBuffer cb = {};
    cb.mvp[0] = cb.mvp[5] = cb.mvp[10] = cb.mvp[15] = 1.0f;
    sd.pSysMem = &cb;
    hr = device->CreateBuffer(&bd, &sd, &cbuf);
    if (FAILED(hr)) return false;

    // PS constant buffer (LayerBuffer at b1)
    bd.ByteWidth = sizeof(PsConstantBuffer);
    PsConstantBuffer cb_ps;
    cb_ps.layerColor[0] = 1.0f; cb_ps.layerColor[1] = 1.0f;
    cb_ps.layerColor[2] = 1.0f; cb_ps.layerColor[3] = 1.0f;
    cb_ps.layerAlpha = 1.0f;
    cb_ps.layerBrightness = 1.0f;
    sd.pSysMem = &cb_ps;
    hr = device->CreateBuffer(&bd, &sd, &cbuf_ps);
    return SUCCEEDED(hr);
}

bool Renderer::CreateSampler() {
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    HRESULT hr = device->CreateSamplerState(&sd, &sampler);
    return SUCCEEDED(hr);
}

void Renderer::Render(int texIndex) {
    if (!context || !swapchain || !rtv) return;

    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    context->ClearRenderTargetView(rtv, clearColor);

    D3D11_VIEWPORT vp = {0, 0, (float)width, (float)height, 0, 1};
    context->RSSetViewports(1, &vp);
    context->OMSetRenderTargets(1, &rtv, nullptr);

    context->VSSetShader(vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &cbuf);
    context->PSSetConstantBuffers(1, 1, &cbuf_ps);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vbo, &stride, &offset);
    context->IASetIndexBuffer(ibo, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(layout);

    if (texIndex >= 0 && texIndex < (int)textures.size() && textures[texIndex].srv) {
        context->PSSetShaderResources(0, 1, &textures[texIndex].srv);
    }
    context->PSSetSamplers(0, 1, &sampler);

    context->DrawIndexed(6, 0, 0);
    swapchain->Present(1, 0);
}

void Renderer::Resize(int w, int h) {
    width  = w;
    height = h;
    if (!swapchain || !device || !context) return;

    if (rtv) { rtv->Release(); rtv = nullptr; }
    swapchain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);

    ID3D11Texture2D* backBuffer = nullptr;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer) {
        device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
        backBuffer->Release();
    }
}

bool Renderer::LoadTexture(const uint8_t* pixels, int w, int h, Texture* out) {
    if (!device || !pixels || !out) return false;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width       = w;
    td.Height      = h;
    td.MipLevels   = 1;
    td.ArraySize   = 1;
    td.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count   = 1;
    td.SampleDesc.Quality = 0;
    td.Usage       = D3D11_USAGE_DEFAULT;
    td.BindFlags   = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem     = pixels;
    sd.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &sd, &tex);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = td.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(tex, &srvDesc, &out->srv);
    tex->Release();
    if (FAILED(hr)) return false;

    out->width  = w;
    out->height = h;
    return true;
}

void Renderer::Destroy() {
    for (size_t i = 0; i < textures.size(); i++) {
        if (textures[i].srv) textures[i].srv->Release();
    }
    textures.clear();
    if (rtv)       { rtv->Release();       rtv = nullptr; }
    if (cbuf_ps)   { cbuf_ps->Release();   cbuf_ps = nullptr; }
    if (cbuf)      { cbuf->Release();      cbuf = nullptr; }
    if (ibo)       { ibo->Release();       ibo = nullptr; }
    if (vbo)       { vbo->Release();       vbo = nullptr; }
    if (sampler)   { sampler->Release();   sampler = nullptr; }
    if (layout)    { layout->Release();    layout = nullptr; }
    if (ps)        { ps->Release();        ps = nullptr; }
    if (vs)        { vs->Release();        vs = nullptr; }
    if (swapchain) { swapchain->Release(); swapchain = nullptr; }
    if (context)   { context->Release();   context = nullptr; }
    if (device)    { device->Release();    device = nullptr; }
}
