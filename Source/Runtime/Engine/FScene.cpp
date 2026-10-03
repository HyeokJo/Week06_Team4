#include "FScene.h"

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

    std::erase(RenderComponents, Prim);
    SceneBVH.RemoveObject(Prim);

    const size_t Index = static_cast<size_t>(Prim->GetSceneIndex());
    CullDataList.erase(CullDataList.begin() + Index);

    //오클루전 대상에서 제거
    OcclusionTargetFlags.erase(OcclusionTargetFlags.begin() + Index);

    // 당겨진 원소들의 인덱스 멤버 갱신
    for (size_t i = Index; i < RenderComponents.size(); ++i)
    {
        RenderComponents[i]->SetSceneIndex(static_cast<int32>(i));
        RenderComponents[i]->SetBatchIndex(static_cast<int32>(i));
    }

    // 파괴될 포인터가 dirty 목록에 남지 않게
    if (Prim->GetBoundDirtyQueued())
    {
        std::erase(DirtyBoundsList, Prim);
        Prim->SetBoundDirtyQueued(false);
    }

    Prim->SetSceneIndex(-1);
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
        Prim->SetBoundDirtyQueued(false);
        Prim->UpdateWorldBounds();
        int32 Index = static_cast<size_t>(Prim->GetSceneIndex());
        CullDataList[Index] = Prim->GetWorldBounds();
        OcclusionTargetFlags[Index] = Prim->IsOcclusionTarget() ? 1 : 0;
    }
    DirtyBoundsList.clear();
}

void FScene::Clear()
{
    RenderComponents.clear();
    CullDataList.clear();
    DirtyBoundsList.clear();
    OcclusionTargetFlags.clear();
    RenderResourceLibrary = nullptr;
}