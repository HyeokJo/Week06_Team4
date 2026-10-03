#include "FEditorApplication.h"

#include "Runtime/CoreUObject/UAnimatedBillboardComp.h"
#include "Runtime/CoreUObject/UBillBoardComp.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/USpotLightComponent.h"
#include "Runtime/Engine/FRayCastingManager.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Math/FMatrix.h"
#include "Runtime/Rendering/FMesh.h"
#include <Windows.h>

#include <algorithm>
#include <cctype>

#include "Runtime/Core/FString.h"

#include "Runtime/Engine/FSceneView.h"

#include "Runtime/Actors/AActor.h"
#include "Runtime/Actors/TestTextActor.h"

#include "Editor/Visualizer/IVisualizer.h"
#include "Editor/Core/FEditor.h"
#include <Editor/UI/Imgui/FImguiStatsWindow.h>
#include <Runtime/CoreUObject/FStatsManager.h>
#include "Runtime/Core/Globals.h"
#include "Runtime/Engine/FEngine.h"

void FEditorApplication::Initialize_ImguiWin32DX11(
    HWND &Window, ID3D11Device *Device, ID3D11DeviceContext *Context) {
  ImguiManager.Initialize_ImplWin32DX11(Window, Device, Context);
}

void FEditorApplication::Initialize_Runtime(FRenderView *RenderView)
{
  this->RenderView = RenderView;

  Editor.Initialize();
  Editor.InitMultiViewport(FEditorViewportClient{});
  Editor.LoadState();
  Editor.SetViewLayout(Editor.State.GetSplitMode());

  // TEMP: 당분간 기본값으로 활성화
  EditorViewportWindow.Toggle(FImguiStatsWindow::EStatsWindow::Unit);
  EditorViewportWindow.Toggle(FImguiStatsWindow::EStatsWindow::FPS);
}

void FEditorApplication::Shutdown() { Editor.Shutdown(); }

void FEditorApplication::Update(float DeltaTime) {
  BeginFrame();
  Tick(DeltaTime);
}

void FEditorApplication::BeginFrame()
{ 
    ImguiManager.NewFrame();
}

void FEditorApplication::Tick(float DeltaTime) {
  ToolBar.Process(Editor, ConsoleWindow, ControlPanelWindow, PropertyWindow);
  EditorViewportWindow.Process(Editor, DeltaTime);
  WorldOutliner.Process(Editor);
  ControlPanelWindow.Process(Editor);
  PropertyWindow.Process(Editor);
  ConsoleWindow.Process(Editor, [this](const char* Command) {ExecuteCommand(Command);});
  ContentsDrawer.Process(Editor);
  Editor.Process();
}

void FEditorApplication::Render() {
  // 렌더 준비
  RenderView->PrepareRender();

  {
      //컬링 준비 시간 기록?
      // 
      //이동한 오브젝트는 월드 AABB 재계산
      GEngine->UpdateWorldBounds();
  }

  Editor.RenderViewports(*RenderView);
  Editor.RenderGizmo(*RenderView);

  ImguiManager.RenderUI();
}

void FEditorApplication::OnWindowSize(UINT Width, UINT Height) {
  // 뷰포트 종횡비 갱신
  for (auto &Viewport : Editor.GetViewports()) {
    const FVector2 SizePixels =
        Viewport.LengthUV *
        FVector2{static_cast<float>(Width), static_cast<float>(Height)};

    auto &Camera = Viewport.ViewportCamera;
    Camera.SetAspectRatio(SizePixels.X / SizePixels.Y);
  }
}

void FEditorApplication::ExecuteCommand(const char* Command) {
    if (!Command) return;

    FString lowerCmd = Command;
    unsigned int NumberArg = 0;

    std::transform(lowerCmd.begin(), lowerCmd.end(), lowerCmd.begin(), ::tolower);

    if (lowerCmd.compare("stat memory") == 0) {
        UE_LOG("Stat Memory Command is executed!");
        EditorViewportWindow.Toggle(FImguiStatsWindow::EStatsWindow::Memory);
    }

    else if (lowerCmd.compare("stat fps") == 0) {
        UE_LOG("Stat FPS Command is executed!");
        EditorViewportWindow.Toggle(FImguiStatsWindow::EStatsWindow::FPS);
    }

    else if (lowerCmd.compare("stat unit") == 0) {
        UE_LOG("Stat unit Command is executed!");
        EditorViewportWindow.Toggle(FImguiStatsWindow::EStatsWindow::Unit);
    }

    else if (lowerCmd.compare("stat none") == 0) {
        UE_LOG("Stat Window is closed!");
        EditorViewportWindow.SetClose();
    }

    else if (lowerCmd.compare("stat cull") == 0)
    {
        UE_LOG("Stat Cull Command is executed!");
        //EditorViewportWindow.Toggle(FImguiStatsWindow::EStatsWindow::Cull);
    }

    else if (lowerCmd.compare("cull") == 0)
    {
        //컬링 토글
        Globals::bEnableFrustumCulling = !Globals::bEnableFrustumCulling;
        UE_LOG("Culling : %s", Globals::bEnableFrustumCulling ? "ON" : "OFF");
    }    

    else {
        UE_LOG("Unknown command: '%s'\n", Command);
        return;
    }
}

