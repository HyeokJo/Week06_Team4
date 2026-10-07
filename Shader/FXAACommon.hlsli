cbuffer FXAAConstants : register(b2)
{
    float2 InvTextureSize;
    float2 ViewMinUV;
    float2 ViewMaxUV;
    float2 FXAAPadding;
};
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
