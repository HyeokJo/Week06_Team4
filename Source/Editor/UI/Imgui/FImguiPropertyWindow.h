#pragma once
#include "Editor/Core/FEditor.h"
#include "ThirdParty/Imgui/imgui.h"

class AActor;
class UActorComponent;
class USceneComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class UTextInstanceComponent;
class UBillBoardComp;
class UAnimatedBillboardComp;
class UMovementComponent;
class URotationMovementComponent;
class UProjectileMovementComponent;

// 선택된 액터의 컴포넌트 속성을 편집하는 창.
class FImguiPropertyWindow final {
public:
	FImguiPropertyWindow() = default;
	~FImguiPropertyWindow() = default;

	//복사 생성 금지
	FImguiPropertyWindow(const FImguiPropertyWindow&) = delete;
	//복사 대입 금지
	FImguiPropertyWindow& operator=(const FImguiPropertyWindow&) = delete;

	void Process(FEditor& Editor);

private:
	// 액터 클래스명과 UUID.
	void ShowActorHeader(const AActor& Actor) const;

	// 소유 컴포넌트를 나열하고 루트를 표시한다. 부착 트리와는 구분한다.
	void ShowComponentHierarchy(FEditor& Editor, AActor& Actor);

	// 목록 상단의 추가 버튼과 클래스 검색 팝업을 그린다.
	void ShowAddComponentMenu(FEditor& Editor, AActor& Actor);
	ImGuiTextFilter ComponentFilter;

	// 컴포넌트마다 접이식 헤더를 만들고 그 안에 상세 속성을 그린다.
	void ShowComponentSections(FEditor& Editor, AActor& Actor);
	void ShowComponentDetails(FEditor& Editor, AActor& Actor, UActorComponent& Comp, bool bIsRoot);

	// Root와 자식 모두 자신의 상대 Transform을 편집한다.
	void ShowTransform(FEditor& Editor, USceneComponent& Comp) const;

	// 이동 관련 컴포넌트 타입별 속성
	void ShowMovementSettings(AActor& Actor, UMovementComponent& Movement) const;
	void ShowRotationMovementSettings(URotationMovementComponent& Movement) const;
	void ShowProjectileMovementSettings(UProjectileMovementComponent& Movement) const;

	// 컴포넌트 타입별 속성
	void ShowTextSettings(UTextInstanceComponent& TextComp) const;
	void ShowBillboardSettings(UBillBoardComp& BillboardComp) const;
	void ShowAnimatedBillboardSettings(UAnimatedBillboardComp& BillboardComp) const;
	void ShowSpotLightSettings(USpotLightComponent& LightComp) const;
	void ShowPointLightSettings(UPointLightComponent& LightComp) const;
	void ShowStaticMeshSettings(AActor& Actor, UStaticMeshComponent& MeshComp, bool bIsRoot) const;

	// 머티리얼의 텍스처 미리보기 겸 드롭 타깃.
	void ShowMaterialSlot(UStaticMeshComponent& MeshComp, int Slot = 0) const;
	void ShowPipelineSlot(UStaticMeshComponent& MeshComp, int Slot = 0) const;
	void ShowTextureSlot(UStaticMeshComponent& MeshComp, int Slot = 0) const;
	void ShowStaticMeshSlot(UStaticMeshComponent& MeshComp) const;

	void ShowApplyAllMaterialSlot(UStaticMeshComponent& MeshComp) const;
	void ShowApplyAllPipelineSlot(UStaticMeshComponent& MeshComp) const;
	void ShowApplyAllTextureSlot(UStaticMeshComponent& MeshComp) const;

	// 창 하단의 기즈모 모드/공간 선택.
	void ShowGizmoSettings(FEditor& Editor) const;
};
