#include "Constants.hlsli"
#include "PostProcessConstants.hlsli"

//  *(-1,1)(0,0)     *(3,1)(2,0)
//  *(-1,-3)(0,2)

// Scene Depth SRV 텍스처
Texture2D<float> SceneDepthTexture : register(t0);

struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

float4 ConvertWolrPos(float4 pos)
{
    const float rawdepth = SceneDepthTexture.Load(int3(pos.xy, 0)).r;
    
    //uv = SV_Position.xy / ViewportSize
    const float2 uv = pos.xy / ViewportSize;
    
    float4 NDC = float4(0.0, 0.0, 0.0, 1.0);
    
    //NDC 좌표 복원
    NDC.x = uv.x * 2 - 1;
    //y는 뒤집기
    NDC.y = 1 - uv.y * 2;
    NDC.z = rawdepth;
    NDC.w = 1;
    
    //NDC로부터 Inverse ViewProjection 행렬곱으로 월드 좌표 구하기
    float4 WorldPos = mul(NDC, InvViewProj);
    WorldPos.xyz = WorldPos.xyz / WorldPos.w;    
    WorldPos.w = 1.0;
    
    return WorldPos;
}

float4 MainPS(PS_IN input) : SV_Target
{    
    //float4 WorldPos = ConvertWolrPos(input.Pos);
    
    const float rawdepth = SceneDepthTexture.Load(int3(input.Pos.xy, 0)).r;
    
    //rawdepth가 1보다 크거나 같으면 조기 종료
    // 조기 종료 하지 않으면 far plane 거리에 수직으로 체크무늬가 생긴다.
    if (rawdepth >= 1.0)
    {
        return float4(0, 0, 0, 1);
    }
    
    //아래거로 쓰면 view z 거리 별로 선이 생김. 색이 반전됨.
    //uv = SV_Position.xy / ViewportSize
    //const float2 uv = input.Pos.xy / ViewportSize;    
    
    const float2 uv = input.UV;
    
    float4 NDC = float4(0.0, 0.0, 0.0, 1.0);
    
    //NDC 좌표 복원
    NDC.x = uv.x * 2 - 1;
    //y는 뒤집기
    NDC.y = 1 - uv.y * 2;
    NDC.z = rawdepth;
    NDC.w = 1;
    
    //NDC로부터 Inverse ViewProjection 행렬곱으로 월드 좌표 구하기
    float4 WorldPos = mul(NDC, InvViewProj);
    WorldPos.xyz = WorldPos.xyz / WorldPos.w;
    WorldPos.w = 1.0;
        
    float returnColor = floor(WorldPos.x) + floor(WorldPos.y) + floor(WorldPos.z);
    returnColor = returnColor % 2.0;
    returnColor = abs(returnColor);
    
    return float4(returnColor, returnColor, returnColor, 1);
    //return color;
}
