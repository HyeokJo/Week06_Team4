// 뷰포트 표면의 최종 컬러를 백버퍼의 뷰포트 영역으로 옮기는 합성 셰이더.
// 표면은 UNORM이고 백버퍼 RTV는 _SRGB이므로 sRGB 인코딩은 이 출력에서 한 번만 일어난다.
// FullScreenTriangleVS와 함께 쓰며, UV는 뷰포트 영역 기준 0~1이다.

Texture2D<float4> ViewportColor : register(t0);
SamplerState ViewportSampler : register(s0);

struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
};

float4 MainPS(PS_IN Input) : SV_Target
{
    // 표면 크기와 영역 크기가 다르면(리사이즈 대기 중) 샘플러가 늘려서 채운다.
    return ViewportColor.Sample(ViewportSampler, Input.UV);
}
