cbuffer FrameConstants : register(b0)
{
    float Time;
    float DeltaTime;
    float2 FramePadding;
}

cbuffer ViewConstants : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float2 ViewportSize;
    float2 ViewPadding;
}

cbuffer ObjectConstants : register(b2)
{
    //row_major float4x4 MVP;
    float3 ColorOverride;
    float ColorOverrideAmount;
    float2 UVScale;
    float2 UVOffset;
    row_major float4x4 World;
    row_major float4x4 InverseWorld;
    float DisableShading;
    float3 ObjectPadding;
}

cbuffer LightConstants : register(b4)
{
    uint DirectionalLightCount;
    uint PointLightCount;
    uint SpotLightCount;
    float AmbientIntensity;
    float3 AmbientColor;
    float LightPadding;
};

struct PointLight
{
    float3 Position;
    float Intensity;
    float3 LightColor;
    float AttenuationRadius;
};

StructuredBuffer<PointLight> PointLightBuffer : register(t1);

float3 GetPointLightDiffuse(float3 WorldPos, float3 Normal)
{
    float3 N = Normal * rsqrt(max(dot(Normal, Normal), 1e-12f));
    float3 Diffuse = 0.0f;
    for (uint i = 0; i < PointLightCount; ++i)
    {
        PointLight Light = PointLightBuffer[i];
        if (Light.AttenuationRadius <= 0.0f)
            continue;
        float3 ToLight = Light.Position - WorldPos;
        float Distance = length(ToLight);
        float NdotL = max(dot(N, ToLight / max(Distance, 1e-6f)), 0.0f);
        float Attenuation = saturate(1.0f - Distance / Light.AttenuationRadius);
        Diffuse += Light.LightColor * Light.Intensity * NdotL * Attenuation;
    }
    return Diffuse;
}
