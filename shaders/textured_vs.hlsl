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
