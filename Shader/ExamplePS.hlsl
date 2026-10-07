#include "Constants.hlsli"

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL; // VS에서 넘어오는 법선
    float3 WorldPos : TEXCOORD1;
};

float4 MainPS(PS_INPUT Input) : SV_Target
{
    float3 BaseColor = lerp(Input.Color.rgb, ColorOverride, ColorOverrideAmount);

    if (DisableShading > 0.5f)
    {
        return float4(BaseColor, Input.Color.a);
    }
    
    float3 Diffuse = GetPointLightDiffuse(Input.WorldPos, Input.Normal);
    float3 Ambient = AmbientColor * AmbientIntensity;

    float3 FinalColor = BaseColor * (Ambient + Diffuse);
    return float4(FinalColor, Input.Color.a);
    
}
