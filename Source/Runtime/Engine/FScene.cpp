#include "FScene.h"
#include "Runtime\Core\Globals.h"
#include <algorithm>

void FScene::SetRenderResourceLibrary(FRenderResourceLibrary* InRenderResourceLibrary)
{
    RenderResourceLibrary = InRenderResourceLibrary;
}

void FScene::AddRenderComponent(UPrimitiveComponent* Prim) {
    if (Prim == nullptr)
        return;

    auto Iter = std::find(RenderComponents.begin(), RenderComponents.end(), Prim);

    if (Iter == RenderComponents.end()) {
        //RenderComponents에 넣기 전에 인덱스 설정
        const int32 NewIndex = static_cast<int32>(RenderComponents.size());
        Prim->SetSceneIndex(NewIndex);
        Prim->SetBatchIndex(NewIndex);

        RenderComponents.push_back(Prim);
        SceneBVH.AddObject(Prim);

        //처음엔 일단 그리자
        CullDataList.push_back(MakeAlwaysVisibleCullData());

        //오클루전 대상에도 추가
        OcclusionTargetFlags.push_back(0);
        MarkBoundsDirty(Prim);
    }
}

void FScene::RemoveRenderComponent(UPrimitiveComponent* Prim) {
    if (Prim == nullptr || Prim->GetSceneIndex() < 0)
        return;

    //TODO 제거할 때 마지막 요소와 교환하는 방식의 Swap and Pop으로 처리하도록 수정할 것

    const auto It = std::find(RenderComponents.begin(), RenderComponents.end(), Prim);
    if (It == RenderComponents.end()) return;
    const size_t Index = static_cast<size_t>(It - RenderComponents.begin());

    std::erase(DirtyBoundsList, Prim);
    Prim->SetBoundDirtyQueued(false);
    SceneBVH.RemoveObject(Prim);

    RenderComponents.erase(It);
    CullDataList.erase(CullDataList.begin() + Index);

    //오클루전 대상에서 제거
    OcclusionTargetFlags.erase(OcclusionTargetFlags.begin() + Index);

    // 당겨진 원소들의 인덱스 멤버 갱신
    for (size_t i = Index; i < RenderComponents.size(); ++i)
    {
        RenderComponents[i]->SetSceneIndex(static_cast<int32>(i));
        RenderComponents[i]->SetBatchIndex(static_cast<int32>(i));
    }

    Prim->SetSceneIndex(-1);
    Prim->SetBatchIndex(-1);
}

bool FScene::AddPointLight(UPointLightComponent* PointLight)
{
    if (PointLights.size() < Globals::MaxPointLights) {
        PointLights.push_back(PointLight);
        WorldLightConstants.PointLightCount = PointLights.size();
        return true;
    }
    else {
        UE_LOG_WARN("Exeeded Max Point Light Count - This PointLightComponent is not Working");
        return false;
    }
}

bool FScene::RemovePointLight(UPointLightComponent* PointLight)
{
    auto Result = std::erase(PointLights, PointLight);
    if (Result == 0) {
        UE_LOG_WARN("No Data in FScene");
        return false;
    }
    else {
        WorldLightConstants.PointLightCount = PointLights.size();
        return true;
    }
}

void FScene::AddFogComponent(UFogComponent* FogComp)
{
    if (FogComp == nullptr)
        return;

    auto Iter = std::find(FogComponents.begin(), FogComponents.end(), FogComp);

    if (Iter == FogComponents.end())
    {
        FogComponents.push_back(FogComp);
    }
}

void FScene::RemoveFogComponent(UFogComponent* FogComp)
{
    if (FogComp == nullptr)
        return;

    const auto It = std::find(FogComponents.begin(), FogComponents.end(), FogComp);
    if (It == FogComponents.end()) return;
    
    FogComponents.erase(It);
}

void FScene::MarkBoundsDirty(UPrimitiveComponent* Prim)
{
    // 씬에 아직 추가 전이거나 이미 대기 중이면 무시
    if (Prim == nullptr || Prim->GetSceneIndex() < 0 || Prim->GetBoundDirtyQueued())
        return;

    Prim->SetBoundDirtyQueued(true);
    DirtyBoundsList.push_back(Prim);
}

void FScene::UpdateDirtyBounds()
{
    for (UPrimitiveComponent* Prim : DirtyBoundsList)
    {
        if (!Prim) continue;
        Prim->SetBoundDirtyQueued(false);
        const int32 Index = Prim->GetSceneIndex();
        if (Index < 0 || static_cast<size_t>(Index) >= RenderComponents.size() ||
            RenderComponents[Index] != Prim)
            continue;

        // WorldBounds는 한 번 계산하고 컬링과 BVH에서 공유한다.
        Prim->UpdateWorldBounds();
        CullDataList[Index] = Prim->GetWorldBounds();
        OcclusionTargetFlags[Index] = Prim->IsOcclusionTarget() ? 1 : 0;
    }
    if (SceneBVH.ShouldRebuild())
        SceneBVH.Build(RenderComponents);
    else
        SceneBVH.RefitObjects(DirtyBoundsList);

    DirtyBoundsList.clear();
}

void FScene::Clear()
{    // 등록된 객체가 살아 있다면 외부에 남는 인덱스도 초기화한다.
    for (UPrimitiveComponent* Component : RenderComponents)
    {
        if (!Component) continue;
        Component->SetSceneIndex(-1);
        Component->SetBatchIndex(-1);
        Component->SetBoundDirtyQueued(false);
    }

    // 기존 Build는 이전 BVH 인덱스와 내부 배열을 정리한다.
    SceneBVH.Build(TArray<UPrimitiveComponent*>{});

    RenderComponents.clear();
    RenderIndices.clear();
    CullDataList.clear();
    DirtyBoundsList.clear();
    OcclusionTargetFlags.clear();
    RenderResourceLibrary = nullptr;
}