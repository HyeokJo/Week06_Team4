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

//public:
//    void Initialize() override;
//    void Register(UScene& InScene) override;
//    void Unregister() override;
//
//    void SetColor(const FVector4& Color, int32 Index = 0);
//
//protected:
//    UFogComponent() = default;
//
//    FVector Color{1.0f, 1.0f, 1.0f};
//    float ColorAmount = 0.0f;

private:
};
