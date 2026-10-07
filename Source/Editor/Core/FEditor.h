#pragma once

#include "Editor/EditorViewport/FEditorViewportClient.h"
#include "Editor/Gizmo/FGizmo.h"
#include "Editor/Core/FEditorState.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/TWeakObjectPtr.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/CoreUObject/UTextInstanceComponent.h"
#include "Runtime/UI/SSplitter.h"
#include "Runtime/Engine/FEngine.h"
#include "Runtime/Rendering/FViewportRenderSurface.h"
#include "Editor/Visualizer/FVisualizerRegistry.h"

enum class EEditorPrimitiveType : uint8 {
  Cube,
  Cylinder,
  Sphere,
  Billboard,
  Spotlight,
};

class FRenderView;

class FEditor {
public:
  FTransform SelectedTransform;
  //FVector SelectedEulerDegDisplay;

  FEditorState State;

  // 피킹 경로 선택 및 측정. 검증이 끝나면 제거한다.
  bool bUseBVHPicking = true;
  bool bHideUI = false;
  // F11. bHideUI가 숨기는 창에 더해 툴바까지 숨긴다.
  bool bZenMode = false;
  bool bShowBenchmark = true;
  double LastPickingMs = 0.0;
  double AccumulatedPickingMs = 0.0;
  int32 PickingAttempts = 0;

  void ResetPickingStats()
  {
    LastPickingMs = 0.0;
    AccumulatedPickingMs = 0.0;
    PickingAttempts = 0;
  }

public:
    void Initialize();
    void Shutdown();

    void Process();
    bool StartPIE();
    [[nodiscard]] bool IsPlaying() const
    {
        return GEngine && GEngine->GetWorld(EWorldType::PIE) != nullptr;
    }    
    // Pause와 Stop은 현재 PIE World를 대상으로 한다.
    void TogglePIEPause();
    void EndPIE();

    [[nodiscard]] bool IsPIEPaused() const
    {
        UWorld* World = GEngine ? GEngine->GetWorld(EWorldType::PIE) : nullptr;
        return World && World->IsPaused();
    }
    // 프로퍼티는 현재 화면에 표시하는 World의 Actor만 편집한다.
    [[nodiscard]] bool CanEditActorProperties(const AActor* Actor) const;

    // PIE 중에는 에디터 UI를 통한 액터·컴포넌트 구조 변경을 제한한다.
    [[nodiscard]] bool CanEditSceneStructure() const { return !IsPlaying(); }

    // 씬 저장 정책은 구조 변경 정책과 별도로 관리한다.
    [[nodiscard]] bool CanSaveScene() const { return !IsPlaying(); }

    // 아웃라이너 버튼과 Delete 키가 같은 삭제 경로를 사용한다.
    bool DeleteSelectedActor();
    void NewScene();
    void SaveWorld(const FString &Path);
    void LoadWorld(const FString &Path);

    void AddViewport(FEditorViewportClient Viewport);
    void InitMultiViewport(FEditorViewportClient Viewport);
    void ResizeView(FEditorState::SplitViewMode mode);
    void DeleteViewport(int32 IndexOfViewport);
    FEditorViewportClient* GetActiveViewport(); // 임시로 0번 반환

    void UpdateCamera();

    bool SelectActor(AActor *Actor);
	bool SelectComponent(UActorComponent* Component);
    void UnSelectActor();
    AActor *GetSelectedActor() const { return SelectedActor.Get(); }
    UActorComponent* GetSelectedComponent() const { return SelectedComponent.Get(); }
    [[nodiscard]] bool ActorSelected() const { return SelectedActor.IsValid() && !bComponentSelection; }
    [[nodiscard]] bool ObjectSelected() const { return SelectedActor.IsValid() &&
        (!bComponentSelection || SelectedComponent.IsValid()); }
    USceneComponent* GetTransformTarget() const;
    [[nodiscard]] bool CanManipulateSelection() const;
    void RefreshSelectedTransform();
    bool ApplySelectedWorldTransform(const FTransform& WorldTransform);
    bool ConsumeComponentFocusRequest(const UActorComponent* Component);

    // 폴더와 검색 팝업이 동일한 클래스 노출 조건을 사용한다.
    static bool IsAddableComponentClass(const UClass* ClassType);
    // 생성·부착·소유 목록 추가·선택을 두 UI의 공통 경로로 처리한다.
    UActorComponent* AddComponentToActor(AActor* Actor, UClass* ClassType, USceneComponent* AttachParent = nullptr);

    [[nodiscard]] TArray<FEditorViewportClient> &GetViewports() {
        return EditorViewports;
    }
    [[nodiscard]] UWorld* GetCurrentWorld() const
    {
        if (!GEngine) return nullptr;

        const FEditorViewportClient* ActiveViewport = &EditorViewports[ActiveViewportIndex];

        if (!ActiveViewport) return nullptr;

        return GEngine->GetWorld(ActiveViewport->GetWorldType());
    }

    void SpawnActorToCurrentScene(UClass* Type, int Count = 1);
    // 피킹 등에서 현재 씬의 렌더링 대상 컴포넌트가 필요할 때 사용
    [[nodiscard]] const TArray<UPrimitiveComponent*>& GetPrimitiveComponents() const;
    FGizmo& GetGizmo() { return Gizmo; }
    FRenderResourceLibrary *GetRendererLibrary();

    void ClearSelectionForGC();

    void SaveState();
    void LoadState();
    void SetViewLayout(FEditorState::SplitViewMode mode);
    UTextInstanceComponent* GetTextcomp() {
        AActor* Actor = GetSelectedActor();
        return Actor && Actor->GetRootComponent() ? SelectedActorTextComp.Get() : nullptr;
    }

    // 활성 뷰포트마다 표면에 렌더링(씬, 후처리, 기즈모)한 뒤 백버퍼 영역에 합성한다.
    void RenderViewports(FRenderView& RenderView);

    //Viewport관련
    static constexpr int32 MaxViewportCount = 4;
    int32 ActiveViewportIndex = 0;
    SWindow* Root = nullptr;
    SWindow Leaf[MaxViewportCount];
    SSplitterH HorizonSplitter; //세로선
    SSplitterH HorizonSplitter2; //세로선
    SSplitterV VerticalSplitter; // 가로선
    FVisualizerRegistry VisualizerRegistry;

private:
    // 씬을 다중으로 가질 수 있도록 구조개선 가능-이경우 에디터쪽에
    // 클래스를 추가해 씬과 FEditorViewportClient들을 연관
    TArray<FEditorViewportClient> EditorViewports;
    // 뷰포트 번호(EditorViewports 인덱스)별 렌더 표면.
    // 클라이언트는 값으로 복사(PIE 백업/복원)되므로 GPU 리소스는 여기서 따로 소유한다.
    FViewportRenderSurface ViewportSurfaces[MaxViewportCount];
    FGizmo Gizmo;
    TWeakObjectPtr<AActor> SelectedActor;
    TWeakObjectPtr<UActorComponent> SelectedComponent;
    bool bComponentSelection = false;
    bool bFocusSelectedComponent = false;
    TWeakObjectPtr<UTextInstanceComponent> SelectedActorTextComp;

    FEditorState StateBeforePIE;
    TArray<FEditorViewportClient> ViewportsBeforePIE;
    int32 ActiveViewportBeforePIE = 0;
    // 임시 최대화된 현재 뷰포트 번호
    int32 MaximizedViewportBeforePIE = -1;

    // 표면이 없거나 영역 크기와 다르면 만든다. bDeferResize면 크기 변경은 미룬다.
    // 이번 프레임에 그릴 수 있으면 true
    bool UpdateViewportSurface(FRenderer& Renderer, const FEditorViewportClient& Viewport,
                               FViewportRenderSurface& Surface, bool bDeferResize);
    // 활성 Leaf가 쓰지 않는 뷰포트 번호의 표면을 해제한다.
    void ReleaseUnusedViewportSurfaces();
};
