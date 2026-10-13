/**
 * HLSL port of deferred/rlvF.glsl for DX_RENDER.
 */

/*[EXTRA_CODE_HERE]*/

// t0-t3/s0-s3 are used by deferredUtil.hlsl when isDeferred=true.
Texture2D diffuseRect : register(t7);
SamplerState diffuseRectSampler : register(s7);

#ifndef LL_INV_PROJ_DECLARED
#define LL_INV_PROJ_DECLARED
uniform float4x4 inv_proj;
uniform float2 screen_res;
#endif

uniform int    rlvEffectMode;
uniform float4 rlvEffectParam1;
uniform float4 rlvEffectParam2;
uniform float2 rlvEffectParam3;
uniform float4 rlvEffectParam4;
uniform float2 rlvEffectParam5;

#define SPHERE_ORIGIN       rlvEffectParam1.xyz
#define SPHERE_DISTMIN      rlvEffectParam2.y
#define SPHERE_DISTMAX      rlvEffectParam2.w
#define SPHERE_DISTEXTEND   rlvEffectParam3
#define SPHERE_VALUEMIN     rlvEffectParam2.x
#define SPHERE_VALUEMAX     rlvEffectParam2.z
#define SPHERE_PARAMS       rlvEffectParam4
#define BLUR_DIRECTION      rlvEffectParam5.xy

float4 getPositionWithDepth(float2 pos_screen, float depth);
float getDepth(float2 pos_screen);

struct PSInput
{
    float4 position : SV_Position;
    float2 vary_fragcoord : TEXCOORD0;
};

float3 blur13(float2 tc, float2 direction)
{
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float2 off1 = float2(1.411764705882353, 1.411764705882353) * direction;
    float2 off2 = float2(3.2941176470588234, 3.2941176470588234) * direction;
    float2 off3 = float2(5.176470588235294, 5.176470588235294) * direction;

    color += diffuseRect.Sample(diffuseRectSampler, tc) * 0.1964825501511404;
    color += diffuseRect.Sample(diffuseRectSampler, tc + off1 / screen_res) * 0.2969069646728344;
    color += diffuseRect.Sample(diffuseRectSampler, tc - off1 / screen_res) * 0.2969069646728344;
    color += diffuseRect.Sample(diffuseRectSampler, tc + off2 / screen_res) * 0.09447039785044732;
    color += diffuseRect.Sample(diffuseRectSampler, tc - off2 / screen_res) * 0.09447039785044732;
    color += diffuseRect.Sample(diffuseRectSampler, tc + off3 / screen_res) * 0.010381362401148057;
    color += diffuseRect.Sample(diffuseRectSampler, tc - off3 / screen_res) * 0.010381362401148057;

    return color.rgb;
}

static const float pi = 3.14159265;

float3 blurVariable(float2 tc, float kernelSize, float2 direction, float strength)
{
    float numBlurPixelsPerSide = kernelSize / 2.0;
    float3 incrementalGaussian;
    incrementalGaussian.x = 1.0 / (sqrt(2.0 * pi) * strength);
    incrementalGaussian.y = exp(-0.5 / (strength * strength));
    incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

    float4 avgValue = float4(0.0, 0.0, 0.0, 0.0);
    float coefficientSum = 0.0;

    avgValue += diffuseRect.Sample(diffuseRectSampler, tc) * incrementalGaussian.x;
    coefficientSum += incrementalGaussian.x;
    incrementalGaussian.xy *= incrementalGaussian.yz;

    [loop]
    for (float i = 1.0; i <= numBlurPixelsPerSide; i += 1.0)
    {
        avgValue += diffuseRect.SampleLevel(diffuseRectSampler, tc - i * direction / screen_res, 0) * incrementalGaussian.x;
        avgValue += diffuseRect.SampleLevel(diffuseRectSampler, tc + i * direction / screen_res, 0) * incrementalGaussian.x;
        coefficientSum += 2.0 * incrementalGaussian.x;
        incrementalGaussian.xy *= incrementalGaussian.yz;
    }

    return (avgValue / coefficientSum).rgb;
}

float3 chromaticAberration(float2 tc, float2 redDrift, float2 blueDrift, float strength)
{
    float3 sourceColor = diffuseRect.Sample(diffuseRectSampler, tc).rgb;
    float3 driftColor;
    driftColor.r = diffuseRect.Sample(diffuseRectSampler, tc + redDrift / screen_res).r;
    driftColor.g = sourceColor.g;
    driftColor.b = diffuseRect.Sample(diffuseRectSampler, tc + blueDrift / screen_res).b;
    return lerp(sourceColor, driftColor, strength);
}

float4 main(PSInput IN) : SV_Target
{
    float2 fragTC = IN.vary_fragcoord.xy;
    float3 fragPosLocal = getPositionWithDepth(fragTC, getDepth(fragTC)).xyz;
    float distance = length(fragPosLocal.xyz - SPHERE_ORIGIN);

    float denom = max(SPHERE_DISTMAX - SPHERE_DISTMIN, 0.00001);
    float effectStrength = SPHERE_VALUEMIN + lerp(0.0, SPHERE_VALUEMAX - SPHERE_VALUEMIN, (distance - SPHERE_DISTMIN) / denom);
    effectStrength = lerp(effectStrength, lerp(0.0, SPHERE_VALUEMIN, SPHERE_DISTEXTEND.x), distance < SPHERE_DISTMIN);
    effectStrength = lerp(effectStrength, lerp(0.0, SPHERE_VALUEMAX, SPHERE_DISTEXTEND.y), distance > SPHERE_DISTMAX);

    float3 fragColor = diffuseRect.Sample(diffuseRectSampler, fragTC).rgb;
    switch (rlvEffectMode)
    {
        case 0:
            fragColor = lerp(fragColor, SPHERE_PARAMS.rgb, effectStrength);
            break;
        case 1:
            fragColor = blur13(fragTC, BLUR_DIRECTION * effectStrength);
            break;
        case 2:
            fragColor = lerp(fragColor, blurVariable(fragTC, SPHERE_PARAMS.x, BLUR_DIRECTION, max(effectStrength, 0.00001)), effectStrength > 0.0 ? 1.0 : 0.0);
            break;
        case 3:
            fragColor = chromaticAberration(fragTC, SPHERE_PARAMS.xy, SPHERE_PARAMS.zw, effectStrength);
            break;
        case 4:
        {
            effectStrength = sign(effectStrength);
            float pixelWidth = max(1.0, round(SPHERE_PARAMS.x * effectStrength)) / screen_res.x;
            float pixelHeight = max(1.0, round(SPHERE_PARAMS.y * effectStrength)) / screen_res.y;
            fragTC = float2(pixelWidth * floor(fragTC.x / pixelWidth), pixelHeight * floor(fragTC.y / pixelHeight));
            fragColor = diffuseRect.Sample(diffuseRectSampler, fragTC).rgb;
            break;
        }
    }

    return float4(fragColor, 0.0);
}
