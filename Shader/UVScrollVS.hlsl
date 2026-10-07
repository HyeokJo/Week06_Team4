#include "Constants.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 WorldPos : TEXCOORD1;
};

PS_INPUT MainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    Output.WorldPos = WorldPosition.xyz;
    Output.Position = mul(WorldPosition, mul(View, Projection));
    Output.Color = Input.Color;
    Output.UV = Input.UV * UVScale + UVOffset;
    Output.UV.x += Time * 0.33f;

    // 월드 공간 법선 변환
    Output.Normal = mul(Input.Normal, transpose((float3x3)InverseWorld));

    return Output;
}
