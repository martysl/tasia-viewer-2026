/**
 * HLSL port of deferred/rlvV.glsl for DX_RENDER.
 */

struct VSInput
{
    float3 position : POSITION;
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
