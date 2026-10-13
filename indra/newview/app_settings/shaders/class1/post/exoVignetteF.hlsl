/**
 * HLSL port of exoVignetteF.glsl for DX_RENDER.
 */

/*[EXTRA_CODE_HERE]*/

Texture2D diffuseRect : register(t0);
SamplerState diffuseRectSampler : register(s0);

uniform float2 screen_res;
uniform float3 vignette;

struct PSInput
{
    float4 position : SV_Position;
    float2 vary_fragcoord : TEXCOORD0;
};

float4 main(PSInput IN) : SV_Target
{
    float4 diff = diffuseRect.Sample(diffuseRectSampler, IN.vary_fragcoord.xy);
    float2 tc = IN.vary_fragcoord - 0.5;
    float vignette_val = 1.0 - dot(tc, tc);
    diff.rgb *= clamp(pow(lerp(1.0, vignette_val * vignette_val * vignette_val * vignette_val * vignette.z, vignette.x), vignette.y), 0.0, 1.0);
    return diff;
}
