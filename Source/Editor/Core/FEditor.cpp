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
#include "Runtime/Engine/ULevel.h"
#include "Runtime/CoreUObject/UActorComponent.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "ThirdParty/Imgui/imgui.h"
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

    // 렌더러(Device)가 정리되기 전에 뷰포트 표면을 먼저 해제한다.
    for (FViewportRenderSurface& Surface : ViewportSurfaces)
    {
        Surface.Release();
    }
}

FRenderResourceLibrary *FEditor::GetRendererLibrary() {
  return &FRenderResourceLibrary::Get();
}

bool FEditor::StartPIE()
{
    if (IsPlaying()) return false;
    UWorld* EditorWorld = GEngine ? GEngine->GetWorld(EWorldType::Editor) : nullptr;
    if (!EditorWorld) return false;

    UWorld* PIEWorld = UWorld::DuplicateWorld(EditorWorld, EWorldType::PIE);
    if (!PIEWorld) return false;

    // Editor 상태를 저장하고 선택 오버레이와 기즈모의 연결을 정리한다.
    SaveState();
    StateBeforePIE = State;
    ViewportsBeforePIE = EditorViewports;
    ActiveViewportBeforePIE = ActiveViewportIndex;
    MaximizedViewportBeforePIE = State.GetSplitMode() != FEditorState::SplitViewMode::SINGLE && Root == &Leaf[0]
        ? Leaf[0].ViewportIndex : -1;
    bPIEEjected = false;
    ClearSelectionForGC();

    // AddWorld가 Activate, BeginPlay, BVH 생성까지 처리한다.
    // 기본 인자는 Editor이므로 PIE를 명시해야 원본 World가 유지된다.
    GEngine->AddWorld(PIEWorld, EWorldType::PIE);
    EditorViewports[ActiveViewportBeforePIE].SetWorldType(EWorldType::PIE);
    return true;
}

void FEditor::Process() {
    if (FInputManager::Get().IsKeyDown(VK_F11))
    {
        bZenMode = !bZenMode;
    }

    if (FInputManager::Get().IsKeyDown(VK_F8) && !ImGui::GetIO().WantTextInput)
    {
        TogglePIEEject();
    }

    if (!PendingComponentDeletion.IsValid() && FInputManager::Get().IsKeyDown(VK_DELETE) && !ImGui::IsAnyItemActive())
    {
        if (UActorComponent* Component = GetSelectedComponent())
            RequestComponentDeletion(Component);
        else
            DeleteSelectedActor();
    }

    // UI 순회가 끝난 시점에 처리하고, 이후 기존 Transform 캐시 갱신을 실행한다.
    ProcessComponentDeletion();
    // 표시 캐시만 갱신
    RefreshSelectedTransform();

    // 씬의 액터 업데이트
    if (SelectedActor && SelectedActor->IsEditorActor()) {
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
    if (!CanEditSceneStructure()) return;
    UnSelectActor();
    GEngine->AddWorld(NewObject<UWorld>());
    State.ResetToDefaults();
    LoadState();
}

void FEditor::SaveWorld(const FString& Path)
{
    UWorld* CurrentWorld = GEngine->GetWorld(EWorldType::Editor);
    //TODO: Editor World일때만 저장이라 이러한 정책을 쓰지만, 다른 월드 타입 저장일때는 처리를 다르게 해줘야 할거임.
    if (!CanSaveScene()) return;
    if (CurrentWorld)
        FWorldSerializer::SaveWorld(Path, CurrentWorld);
}

void FEditor::LoadWorld(const FString& Path)
{
    if (!CanEditSceneStructure()) return;
    // 씬 로드
    FEditorViewportClient* Viewport = GetActiveViewport();
    UWorld* LoadedWorld = FWorldSerializer::LoadWorld(Path, Viewport ? &Viewport->ViewportCamera : nullptr);
    if (!LoadedWorld) return;
    
    ClearSelectionForGC();
    GEngine->AddWorld(LoadedWorld);
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
    if (Actor && !CanEditActorProperties(Actor)) return false;
    ClearSelectionForGC();
    SelectedActor = Actor;

    if (!Actor) return true;
    
	RefreshSelectedTransform();

    if (Gizmo.Mode == EGizmoMode::None) {
      Gizmo.Mode = EGizmoMode::Translate;
    }
    
    UpdateSelectionOverlay();
    return true;
}

void FEditor::UnSelectActor() {
    ClearSelectionForGC();
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
    SelectedComponent = nullptr;
    SelectedActor = nullptr;
    bComponentSelection = false;
    bFocusSelectedComponent = false;
    SelectedTransform = FTransform{};

    Gizmo.EndInteraction();
    Gizmo.HoveredHandle = EGizmoHandle::None;
}

void FEditor::SpawnActorToCurrentScene(UClass* Type, int Size) {
    if (!CanEditSceneStructure()) return;
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

    // View 메뉴로 배치를 바꾼 경우에만 쓰지 않는 표면을 해제한다.
    // 최대화/복원(ApplyPendingViewportMaximize)은 ResizeView만 거치므로 표면이 유지된다.
    ReleaseUnusedViewportSurfaces();
}

void FEditor::RenderViewports(FRenderView& RenderView)
{
    FRenderer& Renderer = RenderView.GetRenderer();

    // 스플리터나 패널을 드래그하는 동안에는 재생성하지 않고 기존 표면을 늘려서 표시한다.
    // 창 테두리 드래그는 Win32 모달 루프라 프레임이 돌지 않으므로, 놓은 뒤 첫 프레임에 한 번 재생성된다.
    const bool bDeferResize = FInputManager::Get().IsMousePressed(EMouseButton::Left);

    //Active인 ViewportClient만 렌더링
    for (SWindow& Lf : Leaf)
    {
        if (!Lf.bisActive)
            continue;

        const int32 ViewportIndex = Lf.ViewportIndex;
        if (ViewportIndex < 0 || ViewportIndex >= MaxViewportCount ||
            ViewportIndex >= static_cast<int32>(EditorViewports.size()))
            continue;

        FEditorViewportClient& Viewport = EditorViewports[ViewportIndex];
        FViewportRenderSurface& Surface = ViewportSurfaces[ViewportIndex];

        if (!UpdateViewportSurface(Renderer, Viewport, Surface, bDeferResize))
            continue;

        // 현재 월드를 받아서 표면에 렌더링한 뒤 백버퍼 영역에 합성
        Viewport.Draw(RenderView, *this, Surface);
    }
}

bool FEditor::UpdateViewportSurface(FRenderer& Renderer, const FEditorViewportClient& Viewport,
                                    FViewportRenderSurface& Surface, bool bDeferResize)
{
    // 합성 영역과 같은 정수 픽셀 크기를 쓴다.
    const D3D11_VIEWPORT Rect = Renderer.GetViewportPixelRect(Viewport.TopLeftUV, Viewport.LengthUV);
    const uint32 Width = static_cast<uint32>(Rect.Width);
    const uint32 Height = static_cast<uint32>(Rect.Height);

    // 영역이 없으면(최소화, 접힌 창) 이번 프레임은 그리지 않는다.
    if (Width == 0u || Height == 0u)
        return false;

    ID3D11Device* Device = Renderer.GetDevice();
    if (!Device)
        return false;

    // 처음 보이는 표면은 기다리지 않고 바로 만든다.
    if (!Surface.IsValid())
        return Surface.Create(*Device, Width, Height);

    if (Surface.GetWidth() == Width && Surface.GetHeight() == Height)
        return true;

    if (bDeferResize)
        return true;

    return Surface.Create(*Device, Width, Height);
}

void FEditor::ReleaseUnusedViewportSurfaces()
{
    bool bUsed[MaxViewportCount] = {};
    for (const SWindow& Lf : Leaf)
    {
        if (Lf.bisActive && Lf.ViewportIndex >= 0 && Lf.ViewportIndex < MaxViewportCount)
        {
            bUsed[Lf.ViewportIndex] = true;
        }
    }

    for (int32 Index = 0; Index < MaxViewportCount; ++Index)
    {
        if (!bUsed[Index])
        {
            ViewportSurfaces[Index].Release();
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
    bPIEEjected = false;
    UWorld* PIEWorld = GEngine ? GEngine->GetWorld(EWorldType::PIE) : nullptr;
    if (!PIEWorld) return;


    EditorViewports[ActiveViewportBeforePIE].SetWorldType(EWorldType::Editor);

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

bool FEditor::CanEditActorProperties(const AActor* Actor) const
{
    UWorld* World = GetCurrentWorld();
    return World && Actor && Actor->GetOwner() && Actor->GetOwner()->GetWorld() == World;
}

bool FEditor::DeleteSelectedActor()
{
    if (!ActorSelected()) return false;
    AActor* Actor = GetSelectedActor();
    if (!CanEditActorProperties(Actor)) return false;
    ClearSelectionForGC();
    Actor->Destroy();
    return true;
}

bool FEditor::SelectComponent(UActorComponent* Component)
{
    if (!Component) return false;

    AActor* Actor = Component->GetActorOwner();
    if (!CanEditActorProperties(Actor)) return false;

    // 다른 Actor의 Component를 선택하면 소유 Actor부터 전환한다.
    if (GetSelectedActor() != Actor && !SelectActor(Actor)) return false;

    // 기존 드래그의 시작 Transform이 새 선택 대상에 적용되지 않도록 종료한다.
    Gizmo.EndInteraction();
    Gizmo.HoveredHandle = EGizmoHandle::None;

    SelectedComponent = Component;
    bComponentSelection = true;
    bFocusSelectedComponent = true;
    RefreshSelectedTransform();

    // 일반 ActorComponent는 선택할 수 있지만 Transform 조작 대상은 아니다.
    if (GetTransformTarget() && Gizmo.Mode == EGizmoMode::None)
        Gizmo.Mode = EGizmoMode::Translate;

    return true;
}

USceneComponent* FEditor::GetTransformTarget() const
{
    AActor* Actor = GetSelectedActor();
    if (!Actor) return nullptr;

    if (bComponentSelection)
    {
        // 선택 Component가 없거나 SceneComponent가 아니면 기즈모 대상이 없다.
        UActorComponent* Component = GetSelectedComponent();
        return Component ? Component->Cast<USceneComponent>() : nullptr;
    }

    return Actor->GetRootComponent();
}

bool FEditor::CanManipulateSelection() const
{
    UWorld* World = GetCurrentWorld();

    // 입력 대상은 활성 뷰의 World에 속해야 하고, Transform 조작이 가능해야 한다.
    return World && CanUseEditorControls(World->GetWorldType()) &&
        CanEditActorProperties(GetSelectedActor()) && GetTransformTarget() != nullptr;
}

void FEditor::RefreshSelectedTransform()
{
    // SelectedTransform은 실제 Component 데이터를 따라가는 표시용 캐시다.
    USceneComponent* Target = GetTransformTarget();
    SelectedTransform = Target ? Target->GetGlobalTransform() : FTransform{};
}

bool FEditor::ApplySelectedWorldTransform(const FTransform& WorldTransform)
{
    if (!CanManipulateSelection()) return false;

    // 월드 목표값을 상대값으로 변환하고 기존 Dirty 처리 경로로 적용한다.
    const bool bApplied = GetTransformTarget()->SetWorldTransform(WorldTransform);

    // 적용 실패 시에도 기즈모 표시를 실제 Component 상태와 일치시킨다.
    RefreshSelectedTransform();
    return bApplied;
}

bool FEditor::ConsumeComponentFocusRequest(const UActorComponent* Component)
{
    if (!bFocusSelectedComponent || GetSelectedComponent() != Component) return false;

    // 해당 섹션이 실제로 그려지는 시점에 요청을 소비한다.
    bFocusSelectedComponent = false;
    return true;
}

bool FEditor::IsAddableComponentClass(const UClass* ClassType)
{
    // 명시적으로 허용한 ActorComponent 계열만 생성 목록에 표시한다.
    return ClassType && ClassType->IsChildOrSelfOf(UActorComponent::StaticClass()) 
        && ClassType->HasMetaValue("SpawnableComponent", "true");
}

UActorComponent* FEditor::AddComponentToActor(AActor* Actor, UClass* ClassType, USceneComponent* AttachParent)
{
    // 현재 World의 Actor에만 추가하며, 다른 Actor의 컴포넌트를 부모로 받지 않는다.
    if (!CanEditActorProperties(Actor) || !IsAddableComponentClass(ClassType) ||
        (AttachParent && AttachParent->GetActorOwner() != Actor))
        return nullptr;

    // 인수 순서는 Outer, ClassType이다. 기본 에셋은 팩토리의 PostInitProperties()가 설정한다.
    UObject* Object = NewObjectWithOuter(Actor, ClassType);
    if (!Object) return nullptr;

    // 위 클래스 검사로 ActorComponent 계열임이 보장된다.
    UActorComponent* Component = Object->Cast<UActorComponent>();

    if (USceneComponent* SceneComponent = Component->Cast<USceneComponent>())
    {
        // 새 컴포넌트는 부모 원점에서 시작한다: 위치 0, 회전 0, 스케일 1.
        SceneComponent->SetRelativeTransform(FTransform{});

        if (USceneComponent* Root = Actor->GetRootComponent())
        {
            USceneComponent* Parent = AttachParent ? AttachParent : Root;

            // 등록 전에 부착 관계를 결정한다. 실패한 객체는 소유 목록에 넣지 않는다.
            if (!SceneComponent->SetupAttachment(Parent))
            {
                DestroyObject(Component);
                return nullptr;
            }
        }
        // Root가 없으면 아래 AddComponent()가 이 컴포넌트를 Root로 지정한다.
    }

    // 기존 경로가 ActorOwner, 소유 목록, Initialize, Register, BeginPlay를 처리한다.
    // 일반 ActorComponent는 부착 부모 없이 소유 목록에만 들어간다.
    Actor->AddComponent(Component);

    // 기존 선택 경로가 Property 섹션 이동과 Editor 기즈모 대상을 갱신한다.
    SelectComponent(Component);
    return Component;
}

void FEditor::ProcessComponentDeletion()
{
    UActorComponent* Component = PendingComponentDeletion.Get();
    PendingComponentDeletion.Reset();
    if (!Component) return;

    AActor* Actor = Component->GetActorOwner();

    // 요청 이후 World가 바뀌었을 수도 있으므로 실행 시점에 검사한다.
    // 현재 PIE World의 Actor도 이 조건을 통과한다.
    if (!CanEditActorProperties(Actor)) return;

    TWeakObjectPtr<USceneComponent> PreviousParent;
    if (USceneComponent* SceneComponent = Component->Cast<USceneComponent>())
        PreviousParent = SceneComponent->GetAttachParent();

    // 기즈모와 외부 소유 UUID 오버레이를 먼저 분리한다.
    ClearSelectionForGC();
    const bool bDeleted = Component->DestroyComponent();

    // 살아 있는 소유 Actor를 다시 선택해 오버레이와 표시 캐시를 연결한다.
    SelectActor(Actor);
    if (!bDeleted) return;

    // 기존 부모, 새 Root, Actor 순으로 선택을 복원한다.
    USceneComponent* NextSelection = PreviousParent.Get();
    if (!NextSelection || NextSelection->GetActorOwner() != Actor)
        NextSelection = Actor->GetRootComponent();

    if (NextSelection) SelectComponent(NextSelection);
}

bool FEditor::CanUseEditorControls(EWorldType WorldType) const
{
    return WorldType == EWorldType::Editor || (WorldType == EWorldType::PIE && bPIEEjected);
}

void FEditor::TogglePIEEject()
{
    if (!IsPlaying()) return;

    // World와 Pause 상태는 유지하고, 입력·표시 허용 상태만 전환한다.
    bPIEEjected = !bPIEEjected;

    // 진행 중인 드래그가 다음 조작 상태까지 이어지지 않도록 종료한다.
    Gizmo.EndInteraction();
    Gizmo.HoveredHandle = EGizmoHandle::None;

    // Actor와 Component 선택은 유지하며, 표시 데이터만 갱신한다.
    RefreshSelectedTransform();
    UpdateSelectionOverlay();
}

void FEditor::UpdateSelectionOverlay()
{
    if (!SelectedActorTextComp) return;

    AActor* Actor = GetSelectedActor();
    ULevel* Level = Actor ? Actor->GetOwner() : nullptr;
    UWorld* World = Level ? Level->GetWorld() : nullptr;
    USceneComponent* RootComponent = Actor ? Actor->GetRootComponent() : nullptr;

    // 일반 PIE로 돌아가면 오버레이 연결만 끊고 선택 자체는 유지한다.
    if (!World || !RootComponent || !CanUseEditorControls(World->GetWorldType()))
    {
        SelectedActorTextComp->SetupDetachment(true);
        return;
    }

    // F8 이전에 선택한 PIE 객체도 탈출 직후 올바른 UUID를 표시한다.
    SelectedActorTextComp->SetupAttachment(RootComponent);
    SelectedActorTextComp->SetInheritRotation(false);

    FTransform RelativeTransform;
    RelativeTransform.SetLocation(FVector{ 0.0f, 0.0f, 1.5f });
    SelectedActorTextComp->SetRelativeTransform(RelativeTransform);
    SelectedActorTextComp->SetText(L"UUID : " + std::to_wstring(Actor->GetUUID()));
}