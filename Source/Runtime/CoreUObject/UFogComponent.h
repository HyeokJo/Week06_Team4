#pragma once

#include "Runtime/Engine/FCamera.h"
#include "Runtime/Engine/FRenderData.h"
#include "Runtime/Engine/ShowFlags.h"
#include "Runtime/Math/FMatrix.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "USceneComponent.h"
#include "Runtime/Engine/FCulling.h"
#include "Runtime/Rendering/FMaterial.h"

class UFogComponent : public USceneComponent {
  GENERATED_BODY()
  DECLARE_UCLASS(UFogComponent, USceneComponent)

public:
  void Initialize() override;
  void Serialize(FArchive& Archive) override;
  void Register(ULevel& InLevel) override;
  void Unregister() override;

  void OnTransformChanged() override;

  FVector4 GetFogColor()const;
  float GetFogDensity()const;
  float GetFogHeightFalloff()const;
  float GetFogMaxOpacity()const;
  float GetStartDistance()const;

  void SetFogColor(FVector& pColor);
  void SetFogDensity(float pDensity);
  void SetFogHeightFalloff(float pFalloff);
  void SetFogMaxOpacity(float pMaxOpacity);
  void SetStartDistance(float pStartDistance);

  //rho0 계산하여 반환
  float Calculaterho0(float CameraLocZ);

private:
	float FogDensity = 0.02f;
	FVector4 FogColor = FVector4(0.273f, 0.410f, 0.593f, 1.f);
	float FogHeightFalloff = 0.2f;
	float FogMaxOpacity = 1.f;
	float StartDistance = 0.f;
};
