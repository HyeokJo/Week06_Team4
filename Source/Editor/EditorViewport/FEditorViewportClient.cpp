#include "FEditorViewportClient.h"
#include "Runtime/Engine/FSceneView.h"
#include "Editor/Core/FEditor.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Engine/FEngine.h"

void FEditorViewportClient::UpdateFocusedAndHovered(bool bFocused, bool bHovered)
{
	this->bFocused = bFocused; this->bHovered = bHovered;
	return;
}

void FEditorViewportClient::SetOrthograpihcView(FEditorViewportClient::EOrthogonalType type)
{
	float distance = 5.0f;
	eOrthogonalType = type;
	ViewportCamera.SetProjectionType(EProjectionType::Orthographic);
	switch (type)
	{
	case EOrthogonalType::ORTHOGRAPHIC_TOP:
		ViewportCamera.SetPosition(FVector(0.0f, 0.0f, distance));
		ViewportCamera.SetRotation(-90.0f, 0.0f);
		break;
	case EOrthogonalType::ORTHOGRAPHIC_BOTTOM:
		ViewportCamera.SetPosition(FVector(0.0f, 0.0f, -distance));
		ViewportCamera.SetRotation(90.0f, 0.0f);
		break;
	case EOrthogonalType::ORTHOGRAPHIC_LEFT:
		ViewportCamera.SetPosition(FVector(0.0f, -distance, 0.0f));
		ViewportCamera.SetRotation(0.0f, 90.0f);
		break;

	case EOrthogonalType::ORTHOGRAPHIC_RIGHT:
		ViewportCamera.SetPosition(FVector(0.0f, distance, 0.0f));
		ViewportCamera.SetRotation(0.0f, -90.0f);
		break;

	case EOrthogonalType::ORTHOGRAPHIC_FRONT:
		ViewportCamera.SetPosition(FVector(distance, 0.0f, 0.0f));
		ViewportCamera.SetRotation(0.0f, 0.0f);
		break;

	case EOrthogonalType::ORTHOGRAPHIC_BACK:
		ViewportCamera.SetPosition(FVector(-distance, 0.0f, 0.0f));
		ViewportCamera.SetRotation(0.0f, 180.0f);
		break;
	}
}

void FEditorViewportClient::Draw(FRenderView& RenderView, FEditor& Editor)
{
	// 뷰포트 렌더링 명세 구성
	FSceneView SceneView{
		.Camera = ViewportCamera,
		.ViewProj = ViewportCamera.GetViewProjectionMatrix(),
		.TopLeftUV = TopLeftUV,
		.LengthUV = LengthUV,
		.ViewMode = ViewMode,
		.ShowFlags = ShowFlags,
		.LightConstants = Editor.GlobalLight
	};

	// 에디터 렌더링 컨텍스트 구성
	FEditorRenderContext EditorCtx;

	//Editor 모드일때만 렌더링.
	if (WorldType == EWorldType::Editor)
	{
		bool bIsEditorActor = Editor.GetSelectedActor() && Editor.GetSelectedActor()->IsEditorActor();
		EditorCtx.SelectedActor = bIsEditorActor ? Editor.GetSelectedActor() : nullptr;
		EditorCtx.SelectedTransform = Editor.SelectedTransform;
		EditorCtx.Gizmo = (Editor.ObjectSelected() && bIsEditorActor) ? &Editor.GetGizmo() : nullptr;
		EditorCtx.TextComp = (Editor.ObjectSelected() && bIsEditorActor) ? Editor.GetTextcomp() : nullptr;
		EditorCtx.Grid = &Grid;
		EditorCtx.VisualizerRegistry = &Editor.VisualizerRegistry;

		if (EditorCtx.SelectedActor && bIsEditorActor) {
			if (USceneComponent* RootComp = EditorCtx.SelectedActor->GetRootComponent()) {
				EditorCtx.SelectedPrimitive = RootComp->Cast<UPrimitiveComponent>();
			}
		}
	}

	if (!GEngine) return;

	UWorld* CurrentWorld = GEngine->GetWorld(WorldType);

	if (!CurrentWorld) return;

	// 뷰포트 렌더링 일괄 수행
	RenderView.RenderView(SceneView, *CurrentWorld, EditorCtx);
}

void FEditorViewportClient::DrawGizmo(FRenderView& RenderView, FEditor& Editor)
{
	// Editor 만 Gizmo 렌더링
	if (WorldType != EWorldType::Editor || !Editor.GetSelectedActor() || !Editor.GetSelectedActor()->IsEditorActor()) return;

	FSceneView SceneView{
		.Camera = ViewportCamera,
		.ViewProj = ViewportCamera.GetViewProjectionMatrix(),
		.TopLeftUV = TopLeftUV,
		.LengthUV = LengthUV,
		.ViewMode = ViewMode,
		.ShowFlags = ShowFlags,
		.LightConstants = Editor.GlobalLight
	};

	// 카메라 상수 버퍼가 갱신이 안됐었음
	//TODO: 더 효율적인 방법이 없나 확인해야함.
	FRenderer& Renderer = RenderView.GetRenderer();
	const FViewConstants ViewConstants{
		.View = SceneView.Camera.GetViewMatrix(),
		.Projection = SceneView.Camera.GetProjectionMatrix(),
		.ViewportSize = FVector2{
			SceneView.LengthUV.X * Renderer.GetWidth(),
			SceneView.LengthUV.Y * Renderer.GetHeight()
		}
	};
	Renderer.UpdateViewConstants(ViewConstants);


	RenderView.RenderOverlayPass(
		ViewportCamera, SceneView, Editor.SelectedTransform, Editor.GetGizmo(), Editor.GetTextcomp()
	);

	// 마지막으로 그린 뷰의 렌더 모드가 남지 않도록 설정
	RenderView.SetRenderMode(ViewMode);
	RenderView.RenderGizmo(
		Editor.SelectedTransform,
		ViewportCamera,
		TopLeftUV,
		LengthUV,
		Editor.GetGizmo()
	);
}