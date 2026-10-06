// 화면 전체를 덮는 큰 삼각형 버텍스 셰이더

//  *(-1,1)(0,0)        *(3,1)(2,0)
//   _________
//  |         |
//  |   NDC   |
//  |_________|


//  *(-1,-3)(0,2)

static const float2 Pos[3] = { float2(-1, 1), float2(3, 1), float2(-1, -3) };
static const float2 UV[3] = { float2(0, 0), float2(2, 0), float2(0, 2) };

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT MainVS(uint id : SV_VertexID)
{
    VS_OUT output;
    output.Pos = float4(Pos[id], 0, 1);
    output.UV = UV[id];
    return output;
}
