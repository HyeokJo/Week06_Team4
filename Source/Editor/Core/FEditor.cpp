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

void FEditor::Shutdown() {
    if (!IsPlaying()) SaveState();
    State.FlushToFile();
}

FRenderResourceLibrary *FEditor::GetRendererLibrary() {
  return &FRenderResourceLibrary::Get();
}

bool FEditor::StartPIE()
{
    if (IsPlaying()) return false;
    UWorld* EditorWorld = GEngine ? GEngine->GetWorld(EWorldType::Editor) : nullptr;
    if (!EditorWorld) return false;

    // 편집 중인 마지막 Transform을 적용한 뒤 복제한다.
    if (SelectedActor) SelectedActor->SetTransform(SelectedTransform);
    UWorld* PIEWorld = UWorld::DuplicateWorld(EditorWorld, EWorldType::PIE);
    if (!PIEWorld) return false;

    // Editor 상태를 저장하고 선택 오버레이와 기즈모의 연결을 정리한다.
    SaveState();
    StateBeforePIE = State;
    ViewportsBeforePIE = EditorViewports;
    ActiveViewportBeforePIE = ActiveViewportIndex;
    MaximizedViewportBeforePIE = State.GetSplitMode() != FEditorState::SplitViewMode::SINGLE && Root == &Leaf[0]
        ? Leaf[0].ViewportIndex : -1;
    ClearSelectionForGC();

    // AddWorld가 Activate, BeginPlay, BVH 생성까지 처리한다.
    // 기본 인자는 Editor이므로 PIE를 명시해야 원본 World가 유지된다.
    GEngine->AddWorld(PIEWorld, EWorldType::PIE);
    return true;
}

void FEditor::Process() {
    if (FInputManager::Get().IsKeyDown(VK_F11))
    {
        bZenMode = !bZenMode;
    }

    // 씬의 액터 업데이트
    if(!IsPlaying())
    {
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
    }

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

    UWorld* World = GetCurrentWorld();
    return World ? World->GetRenderComponents() : Empty;
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
    UWorld* World = GetCurrentWorld();
    if (!World) return;
    for (SWindow& Lf : Leaf)
    {
        if (!Lf.bisActive) 
            continue;
        // 현재 월드를 받아서 렌더링
        EditorViewports[Lf.ViewportIndex].Draw(RenderView, *World, *this);
    }
}

void FEditor::RenderGizmo(FRenderView& RenderView)
{
    if (IsPlaying()) return;
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

void FEditor::TogglePIEPause()
{
    // Pause 중에도 PIE 세션과 렌더링은 유지한다.
    UWorld* World = GEngine ? GEngine->GetWorld(EWorldType::PIE) : nullptr;
    if (World) World->SetPaused(!World->IsPaused());
}

void FEditor::EndPIE()
{
    UWorld* PIEWorld = GEngine ? GEngine->GetWorld(EWorldType::PIE) : nullptr;
    if (!PIEWorld) return;

    // 폐기할 Component를 오버레이와 기즈모가 참조하지 않도록 한다.
    ClearSelectionForGC();

    // 기존 함수가 목록 제거, EndPlay, Deactivate, DestroyObject를 처리한다.
    // 제거된 뒤 GetCurrentWorld()는 다시 Editor World를 반환한다.
    GEngine->RemoveWorld(PIEWorld);

    // 카메라·ShowFlags·그리드와 분할 배치를 시작 전 상태로 복원한다.
    State = StateBeforePIE;
    EditorViewports = std::move(ViewportsBeforePIE);
    ResizeView(MaximizedViewportBeforePIE >= 0 ? FEditorState::SplitViewMode::SINGLE : State.GetSplitMode());

    if (MaximizedViewportBeforePIE >= 0) Leaf[0].ViewportIndex = MaximizedViewportBeforePIE;
    ActiveViewportIndex = ActiveViewportBeforePIE;

    // 원본 Actor의 UUID로 시작 전 선택을 복원한다.
    UWorld* EditorWorld = GEngine->GetWorld(EWorldType::Editor);
    if (EditorWorld)
    {
        for (AActor* Actor : EditorWorld->GetActors())
        {
            if (Actor && Actor->GetUUID() == State.GetSelectedActor())
            {
                SelectActor(Actor);
                break;
            }
        }
    }

    // 선택 과정에서 바뀔 수 있는 기즈모 모드와 기존 설정을 적용한다.
    LoadState();
}