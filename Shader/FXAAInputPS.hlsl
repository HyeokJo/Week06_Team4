#include "FXAACommon.hlsli"

float4 MainPS(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target
{
    float4 color = SceneTexture.Load(int3(pos.xy, 0));
    return float4(LinearToSRGB(color.rgb), color.a);
}
