#include "FXAACommon.hlsli"

// NVIDIA FXAA 백서 기반 고품질 설정: 양방향 32회 탐색, 서브픽셀 trim 1/8, cap 3/4.
static const float FXAA_EDGE_THRESHOLD = 1.0 / 16.0;
static const float FXAA_EDGE_THRESHOLD_MIN = 1.0 / 32.0;
static const float FXAA_SEARCH_THRESHOLD = 1.0 / 4.0;
static const uint FXAA_SEARCH_STEPS = 32;
static const float FXAA_SUBPIX_TRIM = 1.0 / 8.0;
static const float FXAA_SUBPIX_CAP = 3.0 / 4.0;

float FxaaLuma(float3 rgb)
{
    return rgb.r + rgb.g * (0.587 / 0.299) + rgb.b * (0.114 / 0.299);
}
float4 MainPS(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target
{
    float2 posM = pos.xy * InvTextureSize;
    float2 dx = float2(InvTextureSize.x, 0);
    float2 dy = float2(0, InvTextureSize.y);
    float4 rgbM = SampleScene(posM);
    float3 rgbN = SampleScene(posM - dy).rgb;
    float3 rgbW = SampleScene(posM - dx).rgb;
    float3 rgbE = SampleScene(posM + dx).rgb;
    float3 rgbS = SampleScene(posM + dy).rgb;
    float lumaM = FxaaLuma(rgbM.rgb);
    float lumaN = FxaaLuma(rgbN), lumaW = FxaaLuma(rgbW);
    float lumaE = FxaaLuma(rgbE), lumaS = FxaaLuma(rgbS);
    float rangeMin = min(lumaM, min(min(lumaN, lumaW), min(lumaE, lumaS)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaW), max(lumaE, lumaS)));
    float range = rangeMax - rangeMin;
    if (range < max(FXAA_EDGE_THRESHOLD_MIN, rangeMax * FXAA_EDGE_THRESHOLD))
        return float4(SRGBToLinear(rgbM.rgb), rgbM.a);
    float lumaL = (lumaN + lumaW + lumaE + lumaS) * 0.25;
    float blendL = min(FXAA_SUBPIX_CAP, max(0.0, abs(lumaL - lumaM) / range - FXAA_SUBPIX_TRIM) / (1.0 - FXAA_SUBPIX_TRIM));
    float3 rgbNW = SampleScene(posM - dx - dy).rgb;
    float3 rgbNE = SampleScene(posM + dx - dy).rgb;
    float3 rgbSW = SampleScene(posM - dx + dy).rgb;
    float3 rgbSE = SampleScene(posM + dx + dy).rgb;
    float3 rgbL = (rgbN + rgbW + rgbM.rgb + rgbE + rgbS + rgbNW + rgbNE + rgbSW + rgbSE) / 9.0;
    float lumaNW = FxaaLuma(rgbNW), lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW), lumaSE = FxaaLuma(rgbSE);
    float edgeVert = 0.25 * abs(lumaNW - 2 * lumaN + lumaNE) + 0.5 * abs(lumaW - 2 * lumaM + lumaE) + 0.25 * abs(lumaSW - 2 * lumaS + lumaSE);
    float edgeHorz = 0.25 * abs(lumaNW - 2 * lumaW + lumaSW) + 0.5 * abs(lumaN - 2 * lumaM + lumaS) + 0.25 * abs(lumaNE - 2 * lumaE + lumaSE);
    bool horzSpan = edgeHorz >= edgeVert;
    float lengthSign = horzSpan ? -InvTextureSize.y : -InvTextureSize.x;
    if (!horzSpan) { lumaN = lumaW; lumaS = lumaE; }
    float gradientN = abs(lumaN - lumaM), gradientS = abs(lumaS - lumaM);
    lumaN = (lumaN + lumaM) * 0.5;
    lumaS = (lumaS + lumaM) * 0.5;
    if (gradientN < gradientS) { lumaN = lumaS; gradientN = gradientS; lengthSign = -lengthSign; }
    float2 posB = posM + (horzSpan ? float2(0, lengthSign * 0.5) : float2(lengthSign * 0.5, 0));
    float2 offNP = horzSpan ? dx : dy;
    float2 posN = clamp(posB - offNP, ViewMinUV, ViewMaxUV);
    float2 posP = clamp(posB + offNP, ViewMinUV, ViewMaxUV);
    gradientN *= FXAA_SEARCH_THRESHOLD;
    float lumaEndN = lumaN, lumaEndP = lumaN;
    bool doneN = false, doneP = false;
    [loop] for (uint i = 0; i < FXAA_SEARCH_STEPS; ++i)
    {
        if (!doneN) lumaEndN = FxaaLuma(SampleScene(posN).rgb);
        if (!doneP) lumaEndP = FxaaLuma(SampleScene(posP).rgb);
        doneN = doneN || abs(lumaEndN - lumaN) >= gradientN;
        doneP = doneP || abs(lumaEndP - lumaN) >= gradientN;
        if ((doneN && doneP) || i + 1 == FXAA_SEARCH_STEPS) break;
        if (!doneN) posN = clamp(posN - offNP, ViewMinUV, ViewMaxUV);
        if (!doneP) posP = clamp(posP + offNP, ViewMinUV, ViewMaxUV);
    }
    float dstN = horzSpan ? posM.x - posN.x : posM.y - posN.y;
    float dstP = horzSpan ? posP.x - posM.x : posP.y - posM.y;
    float lumaEnd = dstN < dstP ? lumaEndN : lumaEndP;
    if ((lumaM - lumaN < 0) == (lumaEnd - lumaN < 0)) lengthSign = 0;
    float spanLength = dstN + dstP;
    float offset = spanLength > 0 ? (0.5 - min(dstN, dstP) / spanLength) * lengthSign : 0;
    float2 posF = posM + (horzSpan ? float2(0, offset) : float2(offset, 0));
    float3 rgbF = SampleScene(posF).rgb;
    return float4(SRGBToLinear(lerp(rgbF, rgbL, blendL)), rgbM.a);
}
