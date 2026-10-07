#pragma once
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Math/FQuaternion.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "UPrimitiveComponent.h"

class UWorld;
class FJsonArchive;

class UBillBoardComp : public UPrimitiveComponent {
  DECLARE_UCLASS(UBillBoardComp, UPrimitiveComponent)
  GENERATED_BODY()

protected:
  explicit UBillBoardComp() = default;

  virtual void Serialize(FJsonArchive& Archive) const;
  virtual void Deserialize(const FJsonArchive& Archive);

public:
	using Super::Serialize;
  //void Initialize() override;
  void PostInitProperties() override;

  virtual void SetTexture(UTexture* Texture);
  UTexture* GetTexture() const;

  // Object -> World 변환 행렬 생성
  virtual FMatrix GetRenderMatrix(const FCamera& Camera) const override;
  
  virtual EEngineShowFlags GetShowFlag() const { return EEngineShowFlags::SF_BillboardText; }

  void SetUVScale(FVector2 Value);
  void SetUVOffset(FVector2 Value);

  FVector2 GetUVScale() const;
  FVector2 GetUVOffset() const;

  virtual bool IsOcclusionTarget() const override { return false; }
  virtual FAxisAlignedBoundingBox GetLocalBounds() const override;
};

