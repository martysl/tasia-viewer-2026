/**
 * HLSL port of exoPostBaseV.glsl for DX_RENDER.
 * Provides normalized screen coordinates for post-processing effects.
 */

uniform float2 screen_res;

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord0 : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 vary_fragcoord : TEXCOORD0;
};

VSOutput main(VSInput IN)
{
    VSOutput OUT;
    float4 pos = float4(IN.position.xyz, 1.0);
    OUT.position = pos;
    OUT.vary_fragcoord = (pos.xy * 0.5 + 0.5);
    return OUT;
}
