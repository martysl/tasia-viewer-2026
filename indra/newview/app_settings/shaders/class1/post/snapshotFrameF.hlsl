/**
 * HLSL port of snapshotFrameF.glsl for DX_RENDER.
 */

Texture2D diffuseRect : register(t0);
SamplerState diffuseRectSampler : register(s0);

uniform float2 screen_res;
uniform float4 frame_rect;       // x, y, width, height (normalized 0->1)
uniform float3 border_color;
uniform float border_thickness;  // in pixels

struct PSInput
{
    float4 position : SV_Position;
    float2 vary_fragcoord : TEXCOORD0;
};

float4 main(PSInput IN) : SV_Target
{
    float2 uv = IN.vary_fragcoord;
    float4 diff = diffuseRect.Sample(diffuseRectSampler, uv);
    float2 tc = uv * screen_res;

    float4 frame_rect_px = float4(frame_rect.x * screen_res.x, frame_rect.y * screen_res.y,
                                  frame_rect.z * screen_res.x, frame_rect.w * screen_res.y);
    float4 border_rect_px = float4(
        (frame_rect.x * screen_res.x) - border_thickness,
        (frame_rect.y * screen_res.y) - border_thickness,
        (frame_rect.z * screen_res.x) + border_thickness,
        (frame_rect.w * screen_res.y) + border_thickness);

    if (tc.x < border_rect_px.x || tc.x > border_rect_px.z ||
        tc.y < border_rect_px.y || tc.y > border_rect_px.w)
    {
        float3 blur_color = float3(0.0, 0.0, 0.0);
        float blur_size = 2.0;

        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            [unroll]
            for (int y = -1; y <= 1; ++y)
            {
                float2 offset = float2((float)x, (float)y) * blur_size / screen_res;
                blur_color += diffuseRect.SampleLevel(diffuseRectSampler, uv + offset, 0).rgb;
            }
        }

        blur_color /= 9.0;
        float gray = dot(blur_color, float3(0.299, 0.587, 0.114));
        diff.rgb = float3(gray, gray, gray);
    }
    else
    {
        if ((tc.x >= border_rect_px.x && tc.x < frame_rect_px.x) ||
            (tc.x > frame_rect_px.z && tc.x <= border_rect_px.z) ||
            (tc.y >= border_rect_px.y && tc.y < frame_rect_px.y) ||
            (tc.y > frame_rect_px.w && tc.y <= border_rect_px.w))
        {
            diff.rgb = lerp(diff.rgb, border_color, 0.5);
        }
    }

    return diff;
}
