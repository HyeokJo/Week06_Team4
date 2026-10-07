#pragma once
#include "Runtime/Math/FVector2.h"
#include "Runtime/Engine/FCamera.h"
#include "Editor/Grid/FGrid.h"
#include "Runtime/Engine/ShowFlags.h"
#include "Runtime/Engine/UWorld.h"

class FEditor;
class FRenderView;
class FViewportRenderSurface;
struct FSceneView;

class FEditorViewportClient final {
	bool bFocused = false;
	bool bHovered = false;
	FGrid Grid;
	//Grid 이식중, ShowFlag 추가필요
	EWorldType WorldType = EWorldType::Editor;

public:
	// Type에 따라 키보드,마우스 조작이 달라지기 때문에 ViewportClient에 있어야 한다고 생각함
	enum class EOrthogonalType {
		PERSPECTIVE,
		ORTHOGRAPHIC,
		ORTHOGRAPHIC_TOP,
		ORTHOGRAPHIC_BOTTOM,
		ORTHOGRAPHIC_LEFT,
		ORTHOGRAPHIC_RIGHT,
		ORTHOGRAPHIC_FRONT,
		ORTHOGRAPHIC_BACK,
	} eOrthogonalType = EOrthogonalType::PERSPECTIVE; 
	void SetOrthograpihcView(EOrthogonalType type);
	
	FCamera ViewportCamera;
	// 전체 클라이언트 영역 기준 고정 UV: 좌상단 (0,0), 우하단 (1,1).
	// 픽셀 위치/크기는 사용할 때 클라이언트 크기를 곱해 계산한다.
	FVector2 TopLeftUV = { 0.0f, 0.0f };
	FVector2 LengthUV = { 1.0f, 1.0f };

	// 뷰포트 렌더 모드 및 쇼 플래그
	EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
	uint64 ShowFlags = static_cast<uint64>(EEngineShowFlags::SF_Primitives) |
	                   static_cast<uint64>(EEngineShowFlags::SF_BillboardText) |
					   static_cast<uint64>(EEngineShowFlags::SF_Grid) |
					   static_cast<uint64>(EEngineShowFlags::SF_Fog) |
					   static_cast<uint64>(EEngineShowFlags::SF_FXAA);

	FGrid& GetGrid() { return Grid; }
	void UpdateFocusedAndHovered(bool bFocused, bool bHovered);
	const FGrid& GetGrid() const { return Grid; }

	[[nodiscard]] bool HasShowFlag(EEngineShowFlags Flag) const {
		return (ShowFlags & static_cast<uint64>(Flag)) != 0;
	}
	
	void ToggleShowFlag(EEngineShowFlags Flag) {
		ShowFlags ^= static_cast<uint64>(Flag);
	}

	[[nodiscard]] bool IsFocused() const { return bFocused; }
	[[nodiscard]] bool IsHovered() const { return bHovered; }
	[[nodiscard]] EWorldType GetWorldType() const { return WorldType; }

	void Update();
	// 씬, 기즈모를 Surface에 그린 뒤 백버퍼의 이 뷰포트 영역으로 합성한다.
	// Surface는 FEditor가 뷰포트 번호로 소유한다. 이 클래스는 값으로 복사되므로 GPU 리소스를 갖지 않는다.
	void Draw(FRenderView& RenderView, FEditor& Editor, FViewportRenderSurface& Surface);
	void SetWorldType(EWorldType InType) { WorldType = InType; }

private:
	void DrawGizmo(FRenderView& RenderView, FEditor& Editor, const FSceneView& SceneView);
};
