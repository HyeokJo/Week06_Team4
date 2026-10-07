#pragma once

#include "Runtime/Engine/FSceneBVH.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/Core/TArray.h"

class UPointLightComponent;
class UFogComponent;

class FScene final
{
public:
    // Getters
	FSceneBVH& GetSceneBVH() { return SceneBVH; }
	const FSceneBVH& GetSceneBVH() const { return SceneBVH; }
    [[nodiscard]] const TArray<FAxisAlignedBoundingBox>& GetCullDataList() const { return CullDataList; }
    [[nodiscard]] const TArray<UPrimitiveComponent*>& GetRenderComponents() const { return RenderComponents; }
    [[nodiscard]] FRenderResourceLibrary* GetRenderResourceLibrary() const { return RenderResourceLibrary; }
    [[nodiscard]] const TArray<uint8>& GetOcclusionTargetFlags() const { return OcclusionTargetFlags; }
    [[nodiscard]] const FLightConstants GetWorldLightConstants() const { return WorldLightConstants; }
    [[nodiscard]] const TArray<UPointLightComponent*> GetPointLights() const { return PointLights; }
    [[nodiscard]] const TArray<UFogComponent*>& GetFogComponents() const { return FogComponents; }

    void SetRenderResourceLibrary(FRenderResourceLibrary* InRenderResourceLibrary);
    void AddRenderComponent(UPrimitiveComponent* Prim);
    void RemoveRenderComponent(UPrimitiveComponent* Prim);

    bool AddPointLight(UPointLightComponent* PointLight);
    bool RemovePointLight(UPointLightComponent* PointLight);

    void AddFogComponent(UFogComponent* FogComp);
    void RemoveFogComponent(UFogComponent* FogComp);

    // dirty 컴포넌트만 월드 AABB 재계산. 렌더 전에 프레임당 1회
    void UpdateDirtyBounds();
    void MarkBoundsDirty(UPrimitiveComponent* Prim);

    void Clear();

private:
    // 렌더링큐 (Draw용)
    TArray<UPrimitiveComponent*> RenderComponents;
    TMap<UPrimitiveComponent*, size_t> RenderIndices;

    FRenderResourceLibrary* RenderResourceLibrary = nullptr;

    FSceneBVH SceneBVH;

    // 컬링 전용 월드 AABB 배열 (RenderComponents와 같은 인덱스)
    TArray<FAxisAlignedBoundingBox> CullDataList;

    // 이번 프레임 재계산 대상
    TArray<UPrimitiveComponent*> DirtyBoundsList;

    //캐시해둘 오클루전 대상
    TArray<uint8> OcclusionTargetFlags;

    TArray<UPointLightComponent*> PointLights;

    FLightConstants WorldLightConstants;

    //Fog Components
    TArray<UFogComponent*> FogComponents;
};
