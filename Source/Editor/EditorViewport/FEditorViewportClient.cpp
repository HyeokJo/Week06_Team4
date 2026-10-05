#include "FEditorViewportClient.h"
#include "Runtime/Engine/FSceneView.h"
#include "Runtime/Engine/UWorld.h"
#include "Editor/Core/FEditor.h"
#include "Runtime/Engine/FRenderView.h"

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

void FEditorViewportClient::Draw(FRenderView& RenderView, UWorld& InWorld, FEditor& Editor)
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
	if (!Editor.IsPlaying())
	{
		EditorCtx.SelectedActor = Editor.GetSelectedActor();
		EditorCtx.SelectedTransform = Editor.SelectedTransform;
		EditorCtx.Gizmo = Editor.ObjectSelected() ? &Editor.GetGizmo() : nullptr;
		EditorCtx.TextComp = Editor.ObjectSelected() ? Editor.GetTextcomp() : nullptr;
		EditorCtx.Grid = &Grid;
		EditorCtx.VisualizerRegistry = &Editor.VisualizerRegistry;

		if (EditorCtx.SelectedActor) {
			if (USceneComponent* RootComp = EditorCtx.SelectedActor->GetRootComponent()) {
				EditorCtx.SelectedPrimitive = RootComp->Cast<UPrimitiveComponent>();
			}
		}
	}

	// 뷰포트 렌더링 일괄 수행
	RenderView.RenderView(SceneView, InWorld, EditorCtx);
}

void FEditorViewportClient::DrawGizmo(FRenderView& RenderView, FEditor& Editor)
{
	FSceneView SceneView{
		.Camera = ViewportCamera,
		.ViewProj = ViewportCamera.GetViewProjectionMatrix(),
		.TopLeftUV = TopLeftUV,
		.LengthUV = LengthUV,
		.ViewMode = ViewMode,
		.ShowFlags = ShowFlags,
		.LightConstants = Editor.GlobalLight
	};

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