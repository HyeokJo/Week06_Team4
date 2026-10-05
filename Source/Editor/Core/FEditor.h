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
  FVector SelectedEulerDegDisplay;

  // TODO: 이건 Scene에 들어가야함. 아마 아래와 같은 컴포넌트가 부착된 액터로 들어가야할 것
  // https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UDirectionalLightComponent
  FLightConstants GlobalLight;

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
    void UnSelectActor();
    AActor *GetSelectedActor() const { return SelectedActor.Get(); }
    [[nodiscard]] bool ActorSelected() const { return SelectedActor.IsValid(); }
    [[nodiscard]] bool ObjectSelected() const { return SelectedActor.IsValid(); }

    [[nodiscard]] TArray<FEditorViewportClient> &GetViewports() {
        return EditorViewports;
    }
    [[nodiscard]] UWorld* GetCurrentWorld() const
    {
        if (!GEngine) return nullptr;

        // PIE 실행 중에는 화면과 아웃라이너가 복제 World를 사용한다.
        UWorld* PIEWorld = GEngine->GetWorld(EWorldType::PIE);
        return PIEWorld ? PIEWorld : GEngine->GetWorld(EWorldType::Editor);
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
    UTextInstanceComponent* GetTextcomp() { return SelectedActorTextComp; }

    void RenderViewports(FRenderView& RenderView);
    void RenderGizmo(FRenderView& RenderView);
  
    //Viewport관련
    int32 ActiveViewportIndex = 0;
    SWindow* Root = nullptr;
    SWindow Leaf[4];
    SSplitterH HorizonSplitter; //세로선
    SSplitterH HorizonSplitter2; //세로선
    SSplitterV VerticalSplitter; // 가로선
    FVisualizerRegistry VisualizerRegistry;

private:
    // 씬을 다중으로 가질 수 있도록 구조개선 가능-이경우 에디터쪽에
    // 클래스를 추가해 씬과 FEditorViewportClient들을 연관
    TArray<FEditorViewportClient> EditorViewports;
    FGizmo Gizmo;
    TWeakObjectPtr<AActor> SelectedActor;
    TWeakObjectPtr<UTextInstanceComponent> SelectedActorTextComp;

    FEditorState StateBeforePIE;
    TArray<FEditorViewportClient> ViewportsBeforePIE;
    int32 ActiveViewportBeforePIE = 0;
    // 임시 최대화된 현재 뷰포트 번호
    int32 MaximizedViewportBeforePIE = -1;
};
