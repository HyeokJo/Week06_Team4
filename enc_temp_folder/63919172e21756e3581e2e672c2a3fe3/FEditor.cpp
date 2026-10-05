#include "FEditor.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Actors/ACubeActor.h"
#include "Runtime/Actors/ASphereActor.h"
#include "Runtime/Actors/ACylinderActor.h"
#include "Runtime/Actors/ABillboardActor.h"
#include "Runtime/Actors/ASpotlightActor.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/Engine/FTimeManager.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Math/Random.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include <numbers>
#include <Runtime/Engine/FSceneBVH.h>
#include "Runtime/Engine/FWorldSerializer.h"
#include "Runtime/Engine/FRenderView.h"


// 임시 검증에 필요한 객체 목록과 Primitive 접근 함수다.
#include "Runtime/Engine/UWorld.h"
#include "Runtime/CoreUObject/FUObjectArray.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/Core/Log.h"
#include <algorithm>
#include <cassert>


void FEditor::Initialize() {
  State.ReadFromFile();
  Gizmo.Initialize();
  SelectedActorTextComp = NewObject<UTextInstanceComponent>();
  if (SelectedActorTextComp)
  {
    SelectedActorTextComp->Initialize();
    SelectedActorTextComp->SetInheritRotation(false);
    FAssetRegistry& Registry = FAssetRegistry::GetInstance();
    SelectedActorTextComp->SetMesh(Registry.Get<UStaticMesh>("#Rect"));
    SelectedActorTextComp->SetMaterial(Registry.Get<UMaterial>("Material/SelectedActor_Text.json"));
    SelectedActorTextComp->SetFont(FName("bazziotf"));
  }
}

#if defined(_DEBUG)
static void VerifyWorldDuplication(UWorld* SourceWorld)
{
    assert(SourceWorld && SourceWorld->GetLevel());

    FUObjectArray& Objects = FUObjectArray::Get();
    const uint32 BeforeCount = Objects.GetNumObjects();
    const bool bSourceWasActive = SourceWorld->IsActive();

    TMap<const UObject*, UObject*> Duplicates;
    UWorld* CopyWorld = UWorld::DuplicateWorld(
        SourceWorld, EWorldType::PIE, EDuplicateFlags::None, &Duplicates);

    // 복제 직후에는 데이터와 초기화만 준비되어 있어야 한다.
    assert(CopyWorld && CopyWorld != SourceWorld);
    assert(CopyWorld->GetWorldType() == EWorldType::PIE);
    assert(!CopyWorld->IsActive() && !CopyWorld->HasBegunPlay());
    assert(CopyWorld->GetLevel() != SourceWorld->GetLevel());
    assert(CopyWorld->GetLevel()->GetWorld() == CopyWorld);
    assert(CopyWorld->GetActors().size() == SourceWorld->GetActors().size());
    assert(Duplicates.at(SourceWorld) == CopyWorld);
    assert(Duplicates.at(SourceWorld->GetLevel()) == CopyWorld->GetLevel());

    // 현재 복제 대상은 World, Level, 모든 Actor와 소유 Component다.
    size_t ExpectedCount = 2 + SourceWorld->GetActors().size();
    for (AActor* Actor : SourceWorld->GetActors())
        ExpectedCount += Actor->GetAttachedComponents().size();
    assert(Duplicates.size() == ExpectedCount);

    // 폐기 후 원본의 생존 여부를 안전하게 검사하기 위해 UUID를 기록한다.
    TMap<const UObject*, uint32> SourceUUIDs;
    USceneComponent* SourceForEdit = nullptr;
    USceneComponent* CopyForEdit = nullptr;

    for (const auto& [Source, Copy] : Duplicates)
    {
        SourceUUIDs.emplace(Source, Source->GetUUID());

        assert(Source != Copy);
        assert(Source->GetClass() == Copy->GetClass());
        assert(Source->GetUUID() != Copy->GetUUID());

        // 최상위 World를 제외한 모든 Outer는 복제본을 가리켜야 한다.
        if (Source != SourceWorld)
            assert(Copy->GetOuter() == Duplicates.at(Source->GetOuter()));

        if (const AActor* Actor = Source->Cast<AActor>())
        {
            AActor* CopyActor = Copy->Cast<AActor>();
            assert(CopyActor->IsInitialized());
            assert(!CopyActor->IsRegistered() && !CopyActor->HasBegunPlay());

            const auto& Owned = Actor->GetAttachedComponents();
            const auto& CopyOwned = CopyActor->GetAttachedComponents();
            assert(Owned.size() == CopyOwned.size());

            // 전체 소유 목록의 순서와 Root의 참조 대응을 확인한다.
            for (size_t Index = 0; Index < Owned.size(); ++Index)
                assert(CopyOwned[Index] == Duplicates.at(Owned[Index]));
            assert(CopyActor->GetRootComponent() ==
                (Actor->GetRootComponent() ? Duplicates.at(Actor->GetRootComponent()) : nullptr));
        }

        if (const UActorComponent* Component = Source->Cast<UActorComponent>())
        {
            UActorComponent* CopyComponent = Copy->Cast<UActorComponent>();
            assert(CopyComponent->GetActorOwner() == Duplicates.at(Component->GetActorOwner()));
            assert(CopyComponent->IsInitialized());
            assert(!CopyComponent->IsRegistered() && !CopyComponent->HasBegunPlay());
        }

        if (const USceneComponent* Scene = Source->Cast<USceneComponent>())
        {
            USceneComponent* CopyScene = Copy->Cast<USceneComponent>();
            assert(CopyScene->GetRelativeTransform() == Scene->GetRelativeTransform());

            // None 정책에서는 월드 밖의 일반 객체 참조가 nullptr이 된다.
            const auto ParentIt = Duplicates.find(Scene->GetAttachParent());
            USceneComponent* ExpectedParent = ParentIt != Duplicates.end()
                ? ParentIt->second->Cast<USceneComponent>() : nullptr;
            assert(CopyScene->GetAttachParent() == ExpectedParent);

            // 부모의 자식 목록에도 복제된 자식이 정확히 한 번 들어가야 한다.
            if (ExpectedParent)
            {
                const auto& Children = ExpectedParent->GetAttachedComponents();
                assert(std::count(Children.begin(), Children.end(), CopyScene) == 1);
            }

            if (!SourceForEdit)
            {
                SourceForEdit = const_cast<USceneComponent*>(Scene);
                CopyForEdit = CopyScene;
            }
        }

        // Mesh 에셋은 복제하지 않고 공유한다.
        if (const UPrimitiveComponent* Primitive = Source->Cast<UPrimitiveComponent>())
            assert(Copy->Cast<UPrimitiveComponent>()->GetMeshAsset() == Primitive->GetMeshAsset());
    }

    // 복제본의 값을 변경해도 원본 Transform은 유지되어야 한다.
    FTransform BeforeTransform;
    if (SourceForEdit)
    {
        BeforeTransform = SourceForEdit->GetRelativeTransform();
        CopyForEdit->SetRelativeLocation(
            CopyForEdit->GetRelativeLocation() + FVector(100.0f, 0.0f, 0.0f));
        assert(SourceForEdit->GetRelativeTransform() == BeforeTransform);
    }

    // 등록하지 않은 복제 World도 기존 Release 경로로 모두 폐기되어야 한다.
    DestroyObject(CopyWorld);
    Duplicates.clear();

    for (const auto& [Source, UUID] : SourceUUIDs)
        assert(Objects.IsValid(Source, UUID));

    assert(Objects.GetNumObjects() == BeforeCount);
    assert(SourceWorld->IsActive() == bSourceWasActive);
    if (SourceForEdit)
        assert(SourceForEdit->GetRelativeTransform() == BeforeTransform);

    UE_LOG("[WorldDuplicateTest] Passed");
}
#endif



void FEditor::Shutdown() {
  SaveState();
  State.FlushToFile();
}

FRenderResourceLibrary *FEditor::GetRendererLibrary() {
  return &FRenderResourceLibrary::Get();
}

void FEditor::Process() {
#if defined(_DEBUG)
    // F8을 누를 때 현재 Editor World를 복제하고 즉시 검증·폐기한다.
    if (FInputManager::Get().IsKeyDown(VK_F8))
        VerifyWorldDuplication(GEngine->GetWorld(EWorldType::Editor));
#endif
    if (FInputManager::Get().IsKeyDown(VK_F11))
    {
        bZenMode = !bZenMode;
    }

    // 씬의 액터 업데이트
  
    if (FInputManager::Get().IsKeyPressed(VK_DELETE) && SelectedActor)
    {
        AActor* Target = SelectedActor;
        UnSelectActor();
        Target->Destroy();
    }

  if (SelectedActor) {
    SelectedActor->SetTransform(SelectedTransform);
  }

  SaveState();
  State.Tick(FTimeManager::GetDeltaTime());
}

void FEditor::SaveState() {
  const FEditorViewportClient* Viewport = GetActiveViewport();
  if (!Viewport) { return; }

  const FCamera& Camera = Viewport->ViewportCamera;
  State.SetCameraLocation(Camera.GetPosition());
  State.SetCameraPitch(Camera.GetPitch());
  State.SetCameraYaw(Camera.GetYaw());
  State.SetCameraFOV(Camera.GetProjection().GetFOV());
  State.SetGridCellSize(Viewport->GetGrid().GetCellSize());
  State.SetGizmoMode(static_cast<uint8>(Gizmo.Mode));
  State.SetGizmoSpace(static_cast<uint8>(Gizmo.GetSpace()));
  State.SetSelectedActor(SelectedActor ? SelectedActor->GetUUID() : static_cast<uint32>(-1));
}

void FEditor::LoadState()
{
    FEditorViewportClient* Viewport = GetActiveViewport();
    if (!Viewport) { return; }

    FCamera& Camera = Viewport->ViewportCamera;

    Camera.SetPosition(State.GetCameraLocation());
    Camera.SetRotation(State.GetCameraPitch(), State.GetCameraYaw());
    Camera.SetFOV(State.GetCameraFOV());
    Viewport->GetGrid().SetCellSize(State.GetGridCellSize());
    Gizmo.Mode = static_cast<EGizmoMode>(State.GetGizmoMode());
    Gizmo.SetGizmoSpace(static_cast<EGizmoSpace>(State.GetGizmoSpace()));

    //viewmode관련
    VerticalSplitter.Ratio = State.GetSplitter().X;
    HorizonSplitter.Ratio = State.GetSplitter().Y;
    HorizonSplitter2.Ratio = State.GetSplitter().Z;
    
}

void FEditor::NewScene() {
    UnSelectActor();
    GEngine->AddWorld(NewObject<UWorld>());
    State.ResetToDefaults();
    LoadState();
}

void FEditor::SaveWorld(const FString& Path)
{
    UWorld* CurrentWorld = GEngine->GetWorld(EWorldType::Editor);

    if (CurrentWorld)
        FWorldSerializer::SaveWorld(Path, CurrentWorld);
}

void FEditor::LoadWorld(const FString& Path)
{
    // 씬 로드
    FEditorViewportClient* Viewport = GetActiveViewport();
    UWorld* LoadedWorld = FWorldSerializer::LoadWorld(Path, Viewport ? &Viewport->ViewportCamera : nullptr);
    GEngine->AddWorld(LoadedWorld);
    SelectedActor = nullptr;
}

void FEditor::AddViewport(FEditorViewportClient Viewport) {
    EditorViewports.push_back(Viewport);
}

void FEditor::InitMultiViewport(FEditorViewportClient Viewport) {
    EditorViewports.push_back(Viewport);
    EditorViewports.push_back(Viewport);
    EditorViewports.push_back(Viewport);
    EditorViewports.push_back(Viewport);
}
void FEditor::DeleteViewport(int32 IndexOfViewport) {
  EditorViewports.erase(EditorViewports.begin() + IndexOfViewport);
}

FEditorViewportClient* FEditor::GetActiveViewport() {
  if (EditorViewports.empty()) {
    return nullptr;
  }
  return &EditorViewports[ActiveViewportIndex];
}

bool FEditor::SelectActor(AActor *Actor) {
  if (SelectedActor) {
    UnSelectActor();
  }

  SelectedActor = Actor;
  if (SelectedActor) {
    SelectedTransform = SelectedActor->GetTransform();
    SelectedEulerDegDisplay = SelectedTransform.GetRotation().GetEulerXYZ();
    if (Gizmo.Mode == EGizmoMode::None) {
      Gizmo.Mode = EGizmoMode::Translate;
    }

    if (SelectedActorTextComp)
    {
        SelectedActorTextComp->SetupAttachment(SelectedActor->GetRootComponent());
        SelectedActorTextComp->SetInheritRotation(false);
        FTransform RelativeTrans;
        RelativeTrans.SetLocation(FVector{ 0.0f, 0.0f, 1.5f });
        SelectedActorTextComp->SetRelativeTransform(RelativeTrans);
        SelectedActorTextComp->SetText(L"UUID : " + std::to_wstring(SelectedActor->GetUUID()));
    }
  }

  return true;
}

void FEditor::UnSelectActor() {
    if (SelectedActor) SelectedActor->SetTransform(SelectedTransform);

    // 선택 Actor의 소유 컴포넌트가 아니므로 삭제하지 않고 분리한다.
    if (SelectedActorTextComp) SelectedActorTextComp->SetupDetachment(true);
    SelectedActor = nullptr;
}

const TArray<UPrimitiveComponent*>& FEditor::GetPrimitiveComponents() const
{
    static const TArray<UPrimitiveComponent*> Empty;
    UWorld* EditorWorld = GEngine->GetWorld(EWorldType::Editor);

    // TODO: Fix so PIE World can be getted too
    if (!EditorWorld) { return Empty; }

    return EditorWorld->GetRenderComponents();
}

void FEditor::ClearSelectionForGC() {
    // World가 폐기되기 전에 에디터 오버레이의 연결부터 끊는다.
    if (SelectedActorTextComp) SelectedActorTextComp->SetupDetachment(true);
    SelectedActor = nullptr;
    Gizmo.EndInteraction();
    Gizmo.HoveredHandle = EGizmoHandle::None;
}

void FEditor::SpawnActorToCurrentScene(UClass* Type, int Size) {
    UWorld* EditorWorld = GEngine->GetWorld(EWorldType::Editor);

    if (!EditorWorld) { return; }

    if (Size <= 0) { return; }

    const float Min = State.GetSpawnActorMinLocation();
    const float Max = State.GetSpawnActorMaxLocation();
    if (Min > Max) { return; }

    for (int i = 0; i < Size; ++i)
    {
        FVector Location
        {
            Random::GetFloat(Min, Max, 2),
            Random::GetFloat(Min, Max, 2),
            Random::GetFloat(Min, Max, 2),
        };

        AActor* NewActor = EditorWorld->SpawnActorDeferred(Type);
        if (!NewActor) { return; }


        FTransform CurrentTransform = NewActor->GetTransform();
        CurrentTransform.SetLocation(Location);
        CurrentTransform.SetScale3D(FVector{ 0.5f, 0.5f, 0.5f });
        NewActor->SetTransform(CurrentTransform);

        // 액터 시작 및 선택
        EditorWorld->FinishSpawningActor(NewActor);
        SelectActor(NewActor);
    }

    FSceneBVH& BVH = EditorWorld->GetSceneBVH();
    if (BVH.ShouldRebuild())
    {
        BVH.Build(EditorWorld->GetRenderComponents());
    }
}

void FEditor::ResizeView(FEditorState::SplitViewMode mode)
{
    //viewport를 가지고있는 splitter,window를 업데이트
    ActiveViewportIndex = 0;
    //=== 초기화 ===//
    for (int32 i = 0; i < 4; ++i)
    {
        Leaf[i].ViewportIndex = i;
        Leaf[i].bisActive = false;
    }

    HorizonSplitter2.bisActive = false;
    VerticalSplitter.bisActive = false;
    HorizonSplitter.bisActive = false;
    //=== 초기화 ===//

    //===람다함수===//
    auto Connect = [](SSplitter& Splitter, SWindow& LT, SWindow& RB)
        {
            Splitter.SideLT = &LT;
            Splitter.SideRB = &RB;

            Splitter.bisActive = true;
            LT.bisActive = true;
            RB.bisActive = true;
        };

    switch (mode)
    {
        case FEditorState::SplitViewMode::SINGLE:
        Leaf[0].bisActive = true;
        Root = &Leaf[0];
        break;

    case FEditorState::SplitViewMode::HORIZONTAL:
        Leaf[0].bisActive = true;
        Leaf[1].bisActive = true;
        Connect(HorizonSplitter, Leaf[0], Leaf[1]);
        Root = &HorizonSplitter;
        break;

    case FEditorState::SplitViewMode::VERTICAL:
        Leaf[0].bisActive = true;
        Leaf[2].bisActive = true;
        Connect(VerticalSplitter, Leaf[0], Leaf[2]);
        Root = &VerticalSplitter;
        break;

    case FEditorState::SplitViewMode::QUAD:
        Leaf[0].bisActive = true;
        Leaf[1].bisActive = true;
        Leaf[2].bisActive = true;
        Leaf[3].bisActive = true;
        Connect(VerticalSplitter, HorizonSplitter, HorizonSplitter2);
        Connect(HorizonSplitter, Leaf[0], Leaf[1]);
        Connect(HorizonSplitter2, Leaf[2], Leaf[3]);
        Root = &VerticalSplitter;
        break;
    }
}
void FEditor::SetViewLayout(FEditorState::SplitViewMode mode) {
    ResizeView(mode);

    auto SetPerspectiveView = [this](int32 ViewportIndex)
    {
        FEditorViewportClient& Viewport = EditorViewports[ViewportIndex];
        Viewport.eOrthogonalType = FEditorViewportClient::EOrthogonalType::PERSPECTIVE;
        Viewport.ViewportCamera.SetProjectionType(EProjectionType::Perspective);
    };

    auto SetOrthographicView = [this](int32 ViewportIndex, FEditorViewportClient::EOrthogonalType Type)
    {
        EditorViewports[ViewportIndex].SetOrthograpihcView(Type);
    };

    switch (mode)
    {
    case FEditorState::SplitViewMode::SINGLE:
        VerticalSplitter.bisActive = false;
        HorizonSplitter.bisActive = false;
        HorizonSplitter2.bisActive = false;
        SetPerspectiveView(0);
        State.SetSplitMode(FEditorState::SplitViewMode::SINGLE);
        break;

    case FEditorState::SplitViewMode::VERTICAL:
        VerticalSplitter.bisActive = true;
        HorizonSplitter.bisActive = false;
        HorizonSplitter2.bisActive = false;
        SetOrthographicView(0, FEditorViewportClient::EOrthogonalType::ORTHOGRAPHIC_TOP);
        SetPerspectiveView(2);
        State.SetSplitMode(FEditorState::SplitViewMode::VERTICAL);
        break;

    case FEditorState::SplitViewMode::HORIZONTAL:
        VerticalSplitter.bisActive = false;
        HorizonSplitter.bisActive = true;
        HorizonSplitter2.bisActive = false;
        SetOrthographicView(0, FEditorViewportClient::EOrthogonalType::ORTHOGRAPHIC_TOP);
        SetPerspectiveView(1);
        State.SetSplitMode(FEditorState::SplitViewMode::HORIZONTAL);
        break;

    case FEditorState::SplitViewMode::QUAD:
        VerticalSplitter.bisActive = true;
        HorizonSplitter.bisActive = true;
        HorizonSplitter2.bisActive = true;
        SetOrthographicView(0, FEditorViewportClient::EOrthogonalType::ORTHOGRAPHIC_TOP);
        SetPerspectiveView(1);
        SetOrthographicView(2, FEditorViewportClient::EOrthogonalType::ORTHOGRAPHIC_FRONT);
        SetOrthographicView(3, FEditorViewportClient::EOrthogonalType::ORTHOGRAPHIC_RIGHT);
        State.SetSplitMode(FEditorState::SplitViewMode::QUAD);
        break;

    }
}

void FEditor::RenderViewports(FRenderView& RenderView)
{
    //Active인 ViewportClient만 렌더링
    for (SWindow& Lf : Leaf)
    {
        if (!Lf.bisActive) 
            continue;

        FEditorViewportClient& EditorViewport = EditorViewports[Lf.ViewportIndex];
        
        // TODO: Draw PIE World
        UWorld* EditorWorld = GEngine->GetWorld(EWorldType::Editor);

        if (EditorWorld)
            EditorViewport.Draw(RenderView, *EditorWorld, *this);
    }
}

void FEditor::RenderGizmo(FRenderView& RenderView)
{
    if (ObjectSelected())
    {
        for (const SWindow& Lf : Leaf)
        {
            if (!Lf.bisActive)
                continue;

            FEditorViewportClient& Viewport = EditorViewports[Lf.ViewportIndex];
            Viewport.DrawGizmo(RenderView, *this);
        }
    }
}