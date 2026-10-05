#include "Constants.hlsli"
#include "PostProcessConstants.hlsli"

//  *(-1,1)(0,0)     *(3,1)(2,0)
//  *(-1,-3)(0,2)

// Scene Depth SRV 텍스처
Texture2D<float> SceneDepthTexture : register(t0);
// 2컴포넌트 포맷(X24_G8)에 맞게 uint2로 선언
//Texture2D<uint2> StencilTexture : register(t1);



struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

float4 MainPS(PS_IN input) : SV_Target
{
    // Depth SRV의 Depth값
    // 레스터라이저를 거친 Pos는 이미 스크린 좌표이다.
    const float depth = SceneDepthTexture.Load(int3(input.Pos.xy, 0)).r;
    //const float depth = SceneDepthTexture.Load(int3(input.UV, 0)).r;
    
    //depth = A + B/z
    //Projection[][] : 0부터 시작
    //const float A = Projection[2][2];
    //const float B = Projection[2][3];
    
    //projection 행렬이 UE 좌표 기준으로 만들어지고 D3D로 변환되어 들어와서 인덱싱을 바꿔준다.
    const float A = Projection[0][2];
    const float B = Projection[3][2];
    
    //depth = A + B/z
    //위 식을 z로 z = B / (d - A)
    //const float Z = B * (1 / (depth - A)) / FarZ;
    //백버퍼의 색상은 256까지밖에 출력하지 못함에서 오는 Depth View Mode에 계단현상을 완화하기 위해
    //일정 거리마다 Clmap를 걸어서 0 ~ 1의 뎁스 색상이 반복되게 한다.
    float FarClampZ = FarZ / ClampZ;
    const float Z = B * (1 / (depth - A)) % FarClampZ / FarClampZ;
    //const float Z = B * (1 / (depth - A)) % (100 / 10) / (100 / 10);
    
        
    return float4(Z, Z, Z, 1);
}
