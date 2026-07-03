Texture2D    diffuseTexture : register(t0);
SamplerState samplerState  : register(s0);

cbuffer LayerBuffer : register(b1) {
    float4 layerColor;   // rgba tint
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
