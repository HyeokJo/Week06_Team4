#include "Constants.hlsli"

// FXAA는 뷰포트 표면 전체에 그리므로 텍스처 크기 = 표면 크기 = View 상수(b1)의 ViewportSize다.
// 텍셀 크기와 샘플 범위(가장자리 텍셀 중심)를 여기서 계산하므로 별도 상수 버퍼가 필요 없다.
#define InvTextureSize (1.0 / ViewportSize)
#define ViewMinUV (0.5 * InvTextureSize)
#define ViewMaxUV (1.0 - 0.5 * InvTextureSize)

Texture2D<float4> SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);

float3 LinearToSRGB(float3 rgb)
{
    return float3(rgb.r <= 0.0031308 ? 12.92 * rgb.r : 1.055 * pow(max(rgb.r, 0.0), 1.0 / 2.4) - 0.055,
                  rgb.g <= 0.0031308 ? 12.92 * rgb.g : 1.055 * pow(max(rgb.g, 0.0), 1.0 / 2.4) - 0.055,
                  rgb.b <= 0.0031308 ? 12.92 * rgb.b : 1.055 * pow(max(rgb.b, 0.0), 1.0 / 2.4) - 0.055);
}
float3 SRGBToLinear(float3 rgb)
{
    return float3(rgb.r <= 0.04045 ? rgb.r / 12.92 : pow(max((rgb.r + 0.055) / 1.055, 0.0), 2.4),
                  rgb.g <= 0.04045 ? rgb.g / 12.92 : pow(max((rgb.g + 0.055) / 1.055, 0.0), 2.4),
                  rgb.b <= 0.04045 ? rgb.b / 12.92 : pow(max((rgb.b + 0.055) / 1.055, 0.0), 2.4));
}
float4 SampleScene(float2 uv)
{
    return SceneTexture.SampleLevel(SceneSampler, clamp(uv, ViewMinUV, ViewMaxUV), 0);
}
