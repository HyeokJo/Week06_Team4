#include "Constants.hlsli"
Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL; // 법선
    float3 WorldPos : TEXCOORD1;
};

float4 MainPS(PS_INPUT Input) : SV_Target
{
    float4 Sampled = DiffuseTexture.Sample(DiffuseSampler, Input.UV);
    clip(Sampled.a - 0.1f);
    
    // 하이라이트 색상 보간
    float3 Tint = lerp(float3(1.0f, 1.0f, 1.0f), ColorOverride, ColorOverrideAmount);
    float3 BaseColor = Sampled.rgb * Tint;

    if (DisableShading > 0.5f)
    {
        return float4(BaseColor, Sampled.a);
    }
    
    float3 Diffuse = GetPointLightDiffuse(Input.WorldPos, Input.Normal);
    float3 Ambient = AmbientColor * BaseColor * AmbientIntensity;
    float3 FinalColor = Ambient + Diffuse * BaseColor;
    return float4(FinalColor, Sampled.a);
}
