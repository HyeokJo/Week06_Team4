#include "FEditorViewportClient.h"
#include "Runtime/Engine/FSceneView.h"
#include "Editor/Core/FEditor.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Engine/FEngine.h"
#include "Runtime/Engine/ULevel.h"

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

void FEditorViewportClient::Draw(FRenderView& RenderView, FEditor& Editor, FViewportRenderSurface& Surface)
{
	if (!GEngine) return;

	UWorld* CurrentWorld = GEngine->GetWorld(WorldType);
	if (!CurrentWorld) return;

	// 뷰포트 렌더링 명세 구성
	FSceneView SceneView{
		.Camera = ViewportCamera,
		.ViewProj = ViewportCamera.GetViewProjectionMatrix(),
		.TopLeftUV = TopLeftUV,
		.LengthUV = LengthUV,
		.ViewMode = ViewMode,
		.ShowFlags = ShowFlags,
		.LightConstants = CurrentWorld->GetScene()->GetWorldLightConstants()
	};

	// 에디터 렌더링 컨텍스트 구성
	FEditorRenderContext EditorCtx;

	//월드 모드에 따라, 기즈모를 그릴 수 있는가 여부로 편집
	if (Editor.CanUseEditorControls(WorldType))
	{
		Editor.RefreshSelectedTransform();
		AActor* Actor = Editor.GetSelectedActor();
		ULevel* Level = Actor ? Actor->GetOwner() : nullptr;
		const bool bSelectionInThisWorld = Level && Level->GetWorld() == CurrentWorld;

		EditorCtx.SelectedActor = bSelectionInThisWorld ? Actor : nullptr;
		EditorCtx.SelectedTransform = Editor.SelectedTransform;
		EditorCtx.Gizmo = bSelectionInThisWorld && Editor.GetTransformTarget() ? &Editor.GetGizmo() : nullptr;
		EditorCtx.TextComp = bSelectionInThisWorld ? Editor.GetTextcomp() : nullptr;
		EditorCtx.Grid = &Grid;
		EditorCtx.VisualizerRegistry = &Editor.VisualizerRegistry;

		if (bSelectionInThisWorld)
		{
			if (USceneComponent* Target = Editor.GetTransformTarget())
				EditorCtx.SelectedPrimitive = Target->Cast<UPrimitiveComponent>();
		}

	}

	// 씬 ~ 외곽선까지 표면에 렌더링
	RenderView.RenderView(SceneView, *CurrentWorld, EditorCtx, Surface);

	// 기즈모와 오버레이는 외곽선 이후 같은 표면에 그린다. (Fog, 외곽선의 영향을 받지 않음)
	DrawGizmo(RenderView, Editor, SceneView);

	// 표면의 최종 컬러를 백버퍼의 뷰포트 영역으로 합성
	FRenderer& Renderer = RenderView.GetRenderer();
	Renderer.CompositeViewportSurface(Surface, TopLeftUV, LengthUV);
	Renderer.EndViewportSurface();
}

void FEditorViewportClient::DrawGizmo(FRenderView& RenderView, FEditor& Editor, const FSceneView& SceneView)
{
	AActor* Actor = Editor.GetSelectedActor();
	ULevel* Level = Actor ? Actor->GetOwner() : nullptr;
	UWorld* CurrentWorld = GEngine ? GEngine->GetWorld(WorldType) : nullptr;
	if (!Editor.CanUseEditorControls(WorldType) || !CurrentWorld ||
		!Level || Level->GetWorld() != CurrentWorld)
		return;

	Editor.RefreshSelectedTransform();

	// View 상수(b1)는 RenderView에서 이 뷰포트의 카메라로 올린 값이 그대로 남아 있다.

	// 일반 ActorComponent를 선택해도 소유 Actor의 UUID 오버레이는 유지한다.
	RenderView.RenderOverlayPass(
		ViewportCamera, SceneView, Editor.SelectedTransform,
		Editor.GetGizmo(), Editor.GetTextcomp());

	// 활성 뷰의 입력 허용 여부와 관계없이 선택 대상은 표시한다.
	if (!Editor.GetTransformTarget()) return;

	RenderView.SetRenderMode(ViewMode);
	RenderView.RenderGizmo(
		Editor.SelectedTransform, ViewportCamera, Editor.GetGizmo());
}
