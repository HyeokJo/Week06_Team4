#pragma once

#include "Runtime/Math/FMatrix.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Math/FVector4.h"
#include <cstddef>

// Register = b0
struct FFrameConstants {
	float Time;
	float DeltaTime;
	FVector2 Padding;
};
static_assert(sizeof(FFrameConstants) % 16 == 0);

// Register = b1
struct FViewConstants {
	FMatrix View;
	FMatrix Projection;
	FVector2 ViewportSize;
	FVector2 Padding;
};
static_assert(sizeof(FViewConstants) % 16 == 0);

// Register = b2
struct FObjectConstants {
  //FMatrix MVP;
  FVector4 Color{0.0f, 0.0f, 0.0f, 0.0f};
  FVector2 UVScale{1.0f, 1.0f};
  FVector2 UVOffset{0.0f, 0.0f};
  FMatrix World = FMatrix::GetIdentity();
  FMatrix InverseWorld = FMatrix::GetIdentity();
  float DisableShading = 0.0f;
  FVector Padding;

  void SetWorld(const FMatrix& InWorld)
  {
    World = InWorld;
    // Tiny nonzero scales are valid; FMatrix::Inverse's determinant threshold rejects them.
    const DirectX::XMMATRIX Matrix = DirectX::XMLoadFloat4x4(
        reinterpret_cast<const DirectX::XMFLOAT4X4*>(&World.M[0][0]));
    DirectX::XMVECTOR Determinant;
    const DirectX::XMMATRIX Inverse = DirectX::XMMatrixInverse(&Determinant, Matrix);
    assert(DirectX::XMVectorGetX(Determinant) != 0.0f);
    DirectX::XMStoreFloat4x4(
        reinterpret_cast<DirectX::XMFLOAT4X4*>(&InverseWorld.M[0][0]), Inverse);
  }
};
static_assert(sizeof(FObjectConstants) % 16 == 0);
static_assert(sizeof(FObjectConstants) == 176);
static_assert(offsetof(FObjectConstants, InverseWorld) == 96);
static_assert(offsetof(FObjectConstants, DisableShading) == 160);

static constexpr uint32 ConstantRangeAlignment = 256u;

static constexpr uint32 AlignConstantRange(uint32 Size)
{
	return (Size + ConstantRangeAlignment - 1u)
		& ~(ConstantRangeAlignment - 1u);
}

static constexpr uint32 ObjectConstantStride = AlignConstantRange(sizeof(FObjectConstants));

static_assert(ObjectConstantStride % 256u == 0);
static_assert(sizeof(FObjectConstants) <= ObjectConstantStride);

static constexpr uint32 MaxObjectDrawCount = 16384u;

static constexpr uint32 ObjectConstantUploadBufferSize = ObjectConstantStride * MaxObjectDrawCount;

// Register = b3
//struct FShaderConstants {
//
//};
//static_assert(sizeof(FShaderConstants) % 16 == 0);

// Register = b4
struct FLightConstants {
	uint32 DirectionalLightCount = 0;
	uint32 PointLightCount = 0;
	uint32 SpotLightCount = 0;
	float AmbientIntensity{0.4f};
	FVector AmbientColor{1.0f, 1.0f, 1.0f};
	float padding = 0.0f;
};
static_assert(sizeof(FLightConstants) % 16 == 0);

struct FPointLightConstants { // structuredbuffer
	FVector Position{};
	float Intensity = 1.0f;
	FVector LightColor{ 1.0f, 1.0f, 1.0f };
	float AttenuationRadius = 1.0f;
};
static_assert(sizeof(FPointLightConstants) % 16 == 0);

// Register = b2 / ObjectConstants Override
struct FGridConstants {
  FMatrix MVP;
  FMatrix World;
  float CellSize;
  FVector Padding;
};

static_assert(sizeof(FGridConstants) % 16 == 0);

// Register = b2 / ObjectConstants Override
// LINE_LIST 기반 에디터 그리드용 상수 버퍼
struct FGridLineConstants {
  FMatrix MVP;
  FVector CameraPosition;
  float FadeStartDistance;
  float FadeEndDistance;
  FVector Padding;
};

static_assert(sizeof(FGridLineConstants) % 16 == 0);



// PostProcess에서 쓸 Constant Buffer
// Register = b5
struct FSceneDepthConstants
{
	//Scene Depth를 계산하기 위해 필요한 값들
	float FarZ;

	//Scene Depth View에서 0 ~ 1의 표현 Clamp 거리
	float ClampZ = 50.f;

	FVector2 padding;
};

static_assert(sizeof(FSceneDepthConstants) % 16 == 0);
