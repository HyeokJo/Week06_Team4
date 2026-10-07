#include "Constants.hlsli"
#include "PostProcessConstants.hlsli"

//프리 멀티플라이드 설정
//Src * 1 + Dest * (1 - a)

//  *(-1,1)(0,0)     *(3,1)(2,0)
//  *(-1,-3)(0,2)

cbuffer FogConstants : register(b6)
{
    float4 FogColor;
    float FogDencity;
}


// Scene Depth SRV 텍스처
Texture2D<float> SceneDepthTexture : register(t0);

struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

static float4 TempFogColor = float4(1.0, 0.0, 0.0, 1.0);
// 자연로그 2 (ln 2) 값 정의
static const float ln2 = 0.69314718f;

struct FFogParams
{
    float Density;
    float Falloff;
    float Height;
};

float4 RestoreWorldPos(float rawdepth, float2 uv)
{
    //월드 좌표 복원
    float4 NDC = float4(0.0, 0.0, 0.0, 1.0);
    
    //NDC 좌표 복원
    NDC.x = uv.x * 2 - 1;
    //y는 뒤집기
    NDC.y = 1 - uv.y * 2;
    NDC.z = rawdepth;
    NDC.w = 1;
    
    //NDC로부터 Inverse ViewProjection 행렬곱으로 월드 좌표 구하기
    float4 RestoreWorldPos = mul(NDC, InvViewProj);
    RestoreWorldPos.xyz = RestoreWorldPos.xyz / RestoreWorldPos.w;
    RestoreWorldPos.w = 1.0;
    
    return RestoreWorldPos;
}

//높이를 제외한 거리 기반 Fog
float4 DistanceBasedFog(float4 D_WorldPos)
{
    //카메라와의 거리
    float d = length(D_WorldPos.xyz - CameraPos);
    
    //거리가 클수록 안개가 짙도록 : 먼곳을 볼때는 더 많은 안개 입자가 쌓이니 더 짙은 안개
    //거리가 같은 픽셀들 끼리는 같은 tau를 가짐.
    //가까운 곳은 맑고, 먼곳은 짙어진다.
    //float tau = FogDencity * d;
    float tau = 0.05 * d;
    
    //투과율
    //안개를 뚫고 살아남은 빛의 비율
    //거리가 같으면 T가 같다. 거리가 2배가 되면 T는 제곱
    float T = exp(-tau);
    
    //안개량
    float f = 1 - T;
        
    //프리 멀티플라이드 블렌드
    //알파를 미리 곱해서 출력한다.
    // color * 알파, 알파
    return float4(TempFogColor.rgb * f, f);
}

float GetPointHeight(float C_Height, float W_Height, float d, float s)
{
    //h = 시작 높이 + 추가 높이 = h0 + dz * (s / d) = h0 + (dz / d) * s
    
    float dz = W_Height - C_Height;
    return C_Height + (dz / d) * s;
}

float HeightDensity(float C_Height, float W_Height, float Comp_Height, float Falloff, float Distance, float Density, float s)
{
    //ρ(h) = D · 2^( −k · (h − H) ) = D * e^(-k * ln2 * (h - H))
    // h = h0 + (dz/d) * s
    // dz = h1 - h0 = W_Height - C_Height
    //대입하면
    //  p(s) = D * e^(-k * ln2 * ((h0 + (dz/d) * s) - H))
    //지수합은 지수곱으로 나뉘는 성질
    //  a^(m + n) = a^m * a^n
    //적용하면
    //  p(s) = D * 2^(-k * (h0 - H)) * 2^(-k * (dz / d) * s)
    //       = D * e^(-k * (h0 - H) * ln2) * e^(-k * ln2 * (dz / d) * s)
    
    // D : FogDensity                   : 기준 높이 H에서의 밀도 (사용자 지정 밀도)
    // H : FogHeight                    : 기준 높이(컴포넌트의 월드 Z(UE의 Z))
    // k : FogHeightFalloff             : 높이 올라갈 때 밀도가 줄어드는 속도
    // h : 시선 위 임의 지점의 높이       : 시선 위를 움직이며 변하는 값
    
    //  (카메라 높이 h0) ------------ (픽셀 높이 h1)
    //  시선 위의 점 : 카메라에서 s만큼 간 곳.
    //  h(s) = h0 + (h1 - h0)(s/d)
    //       = 시작 높이 + (높이 차이)(총거리 d 중 s만큼 비율)
    //  h(s)는 카메라에서 해당 방향으로 s만틈 이동한 점의 월드 z이다.
    //  카메라에서 보이는 모든 공간상의 월드 z를 구하는거라고 볼 수 있다.
    
    //  h - H
    //  H는 기준 높이 = 컴포넌트의 월드 Z이다.
    //  h = H라면 0이고, h가 H보다 높다면 양수, 낮다면 음수이다.
    //  기준 높이에서 카메라 시선 위 점의 높이 차이이다.
    //  기준 높이에서 위로 올라갈 수록 h - H는 커진다.
    
    //  -k (h - H)
    //  k는 올라갈수록 줄어드는 밀도의 정도이다.
    //  h - H는 올라갈수록 값이 커지기에 -k를 곱해서 올라갈수록 줄어드는 값이 커지도록 한다.
    
    //  2의 거듭제곱 : 일정 거리마다 안개 밀도가 절반이 되게 하기 위해서 이다.
    //  2^(-1) = 0.5이다.
    //  -k * (h - H) = 1 이 되려면 (h - H) = 1/k이다.
    //  기준 높이에서 1/k 만큼 올라가면 밀도가 0.5가 된다는 뜻이다.
    //  2^(-2) = 0.25 인데, h - H가 2/k가 될 때 밀도가 0.25가 된다는 뜻이다.
    //  즉, 높이가 1/k 올라갈 때마다 밀도는 절반이 된다.
    //  k는 안개가 절반으로 옅어지는 높이의 역수가 된다.
    
    //  D를 곱하는 이유
    //  사용자 지정 Fog Density의 값은 기준 높이에서 설정한 밀도이다.
    //  기준 밀도에서 계산해보면 h = H이므로 h - H = 0
    //  p(h) = D * 2^(0) = D가 된다. 기준 밀도에서는 사용자가 지정한 밀도 D로 그대로 나온다.
    //  기준 밀도 D에서 위로 올라갈수록 h의 값이 커지면서 k에 비례하게 밀도가 줄어들게 된다.
    float dz = W_Height - C_Height;
    //return Density * exp2(-Falloff * (GetPointHeight(C_Height, W_Height, Distance, s) - Comp_Height));
    return Density * exp(-Falloff * (C_Height - Comp_Height) * ln2) * exp(-Falloff * (dz / Distance) * s * ln2);

}

//카메라 높이 밀도
float Getrho0(float C_Height, float W_Height, float Comp_Height, float Falloff, float Distance, float Density, float s)
{
    float h0_H = C_Height - Comp_Height;
    
    //h0는 카메라 높이, H는 컴포넌트 높이
    //카메라가 컴포넌트보다 훨씬 아래에 있을 때 h0 - H가 큰 음수일 때
    //지수로 너무 큰 값이 들어가서 float 범위를 넘기다.
    h0_H = max(h0_H, -80);
    
    //p(0) = D * 2^(-k* (h0 - H))
    return Density * exp2(-Falloff * h0_H);
}

//x값이 0에 가까운 값인 경우 나누기 0이 되기에 테일러 전개를 통해 근사값을 계산한다.
float GetTaylor_Exp(float x)
{
    //e(x) = 1 + x + x^2/2 + x^3/6....
    return 1 + x + x * x / 2 + x * x * x / 6;
}

//높이를 포함한 Exponential Height Fog
float4 HeightFog(float4 H_WorldPos)
{
    //밀도 tau = ρ(0) * d * g(x)
    
    //  밀도가 위로 올라갈 수록 줄어들어야 한다.
    //  기준 높이(컴포넌트 높이)에서 위로 갈수록 밀도 값이 줄어야 한다.
    //  기준 높이 H, 카메라부터 픽셀까지의 시선 중 임의 지점의 높이 h
    //  h - H가 커질수록 밀도가 줄어야한다.
    //  2^( −k · (h − H) ) : 높이 올라갈 때 밀도가 줄어드는 속도인 k(Falloff)와 곱하여 지수적으로 줄어들도록 한다.
    //  p(h) = D * 2^(−k · (h − H))
    
    //  h = 시작 높이 + 추가 높이 = h0 + dz * (s/d) = h0 + (dz/d) * s
    //  h는 시작 높이(카메라 높이 h0)와 총 길이 d, 총 z 변화값(dz)와 시선 위 거리 변화 s로 나타낼 수 있다.
    //  p(s) = D * 2^(-k(h - H))
    //       = D * 2^(-k(h0 + (dz/d) * s - H))
    //  지수의 합은 곱으로 나뉘는 성질에 의해 나눌 수 있다. a^(m + n) = a^m * a^n
    //  p(s) = D * 2^(-k(h0 + (dz/d) * s - H)) = D * 2^(-k(h0 - H) * 2^(-k * (dz/d) * s)
    //  앞단의 D * 2^(-k(h0 - H)는 p(0)의 값이다.
    
    //정리하면 p(s) = p(0) * 2^(-k * (dz/d) * s)
    
    //  2^y = e^(yln2)라는 성질을 이용하여
    //  2^(-k * (dz/d) * s) = e^(-k * ln2 * (dz/d) * s)    
    //  a가 k * ln2 * (dz/d) 일 때 
    //  p(s) = p(0) * e^(-a * s)
    
    //s가 0 -> d로 갈 때 모든 밀도의 합인 tau를 구해야 한다.
    //  미분할 때 밀도 p(s)가 나오는 함수 F(s)를 찾는다.
    //  끝 시점의 밀도와 시작 시점의 밀도의 차이가 총 밀도이다.
    //  F(s) = -(1/a) * e^(-ad)
    //  S s=0->d p(s) ds = S s=0->d p(0) * e^(-a*s) ds = F(d) - F(0) 
    //                   = (1/a)(1 - e^(-ad))
    //  tau = p(0)(1/a)(1 - e^(-ad))
    //  ad = k*ln2*dz 일 때 = x라고 하면 1/a = d/x
    //  tau = p(0) * d * (1 - e^(-x))/x
    //  g(x) = (1 - e^(-x))/x 일 때 tau = p(0) * d * g(x)
        
    //높이에 따라 밀도의 변화를 추가한다.
    //낮을수록 짙고, 높을수록 옅다.
    
    //카메라 높이
    float CameraHeight = CameraPos.z;
    
    //픽셀의 월드 높이
    float WorldHeight = H_WorldPos.z;
    
    //카메라와의 거리
    float d = length(H_WorldPos.xyz - CameraPos);
    
    //임시 값. 외부로 뺄 것.
    //높이 올라갈 때 밀도가 줄어드는 속도
    float FogHeightFalloff = 0.02;
    
    //임시 밀도. 외부로 뺄 것.
    float Density = 0.02;
    
    //임시 컴포넌트 Height
    float ComponentHeight = 0;
    
    //안개 밀도
    //float tau0 = HeightDensity(CameraHeight, WorldHeight, ComponentHeight, FogHeightFalloff, d, Density, 0);
    
    //위와 같은 결과이지만 Getrho0에서는 dz/d의 수식이 없다.
    //distance가 0일 경우 위험성을 배제한 수식이다.
    float tau0 = Getrho0(CameraHeight, WorldHeight, ComponentHeight, FogHeightFalloff, d, Density, 0);
    
    //x = ad = k*ln2*dz/d * d = k*ln2*dz
    float x = FogHeightFalloff * ln2 * (WorldHeight - CameraHeight);
    
    
    //-80보다 작은 값이라면 exp(-x)에 들어갈 때 문제된다.
    //대략 exp(88)이 float 한계 범위
    x = max(x, -80);
    
    
    //0에 가까운 값이라면 근사값으로 적용
    //x가 0에 가깝다는건 worldHeight - cameraHeight가 0에 가깝다는 것. 카메라와 픽셀이 수평을 이룬다는 것
    //exp(-x)의 -x가 커지면 오버플로난다.
    //exp(88)가 float 한계 근처
    float ExpX = 0;
    if (abs(x) < 0.01)
    {
        ExpX = GetTaylor_Exp(-x);
    }
    else
    {
        ExpX = exp(-x);
    }
    
    //g(x) = (1 - e^(-x))/x    
    float g_x = (1 - ExpX) / x;
    
    float tau = tau0 * d * g_x;
    
    //투과율
    //안개를 뚫고 살아남은 빛의 비율
    //거리가 같으면 T가 같다. 거리가 2배가 되면 T는 제곱
    float T = exp(-tau);
    
    //안개량
    float f = 1 - T;
        
    //프리 멀티플라이드 블렌드
    //알파를 미리 곱해서 출력한다.
    // color * 알파, 알파
    return float4(TempFogColor.rgb * f, f);
}

float4 MainPS(PS_IN input) : SV_Target
{
    const float rawdepth = SceneDepthTexture.Load(int3(input.Pos.xy, 0)).r;
    const float2 uv = input.UV;
    
    //rawdepth가 1보다 크거나 같으면 Far Z에 해당하는 거리이다.
    // 최대 안개량으로 설정한다.
    if (rawdepth >= 1.0)
    {
        //Src * 1 + Dest * (1 - a)
        //return 0,0,0,0
        //  (0,0,0) + Dest * 1 = Dest로 RT 색상으로 나간다
        //return float4(0, 0, 0, 0);
    }
    
    //월드 좌표 복원
    float4 WorldPos = RestoreWorldPos(rawdepth, uv);
    
    //거리 기반 Fog
    //return DistanceBasedFog(WorldPos);
    
    //높이 Fog
    return HeightFog(WorldPos);
}
