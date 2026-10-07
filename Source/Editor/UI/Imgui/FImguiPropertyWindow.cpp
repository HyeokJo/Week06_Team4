#include "FImguiPropertyWindow.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/USpotLightComponent.h"
#include "Runtime/CoreUObject/PointLightComponent.h"
#include "Runtime/CoreUObject/UTextInstanceComponent.h"
#include "Runtime/CoreUObject/UBillBoardComp.h"
#include "Runtime/CoreUObject/UAnimatedBillboardComp.h"
#include "Runtime/CoreUObject/Mesh/UStaticMeshComponent.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Actors/AActor.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "ThirdParty/Imgui/imgui_impl_dx11.h"
#include "ThirdParty/Imgui/imgui_impl_win32.h"
#include "ThirdParty/Imgui/imgui_stdlib.h"
#include <string>
#include <algorithm>
#include "FImguiDragDrop.h"
#include "Runtime/Rendering/FMaterial.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include "Runtime/Asset/UFont.h"
#include "Runtime/CoreUObject/URotationMovementComponent.h"
#include "Runtime/CoreUObject/UProjectileMovementComponent.h"

namespace
{
	constexpr float SlotSize = 64.0f;
}


void FImguiPropertyWindow::Process(FEditor& Editor)
{
	if (Editor.bHideUI || Editor.bZenMode)
	{
		return;
	}

	if (!ImGui::Begin("Jungle Property Window"))
	{
		ImGui::End();
		return;
	}

	if (AActor* SelectedActor = Editor.GetSelectedActor())
	{
		ShowActorHeader(*SelectedActor);
		ImGui::Separator();

		ShowComponentHierarchy(Editor, *SelectedActor);
		ImGui::Separator();
		ShowComponentSections(Editor, *SelectedActor);

		if (!SelectedActor->GetRootComponent())
		{
			ImGui::TextDisabled("No RootComponent");
		}
	}
	else
	{
		ImGui::TextDisabled("No selection");
	}
	const FEditorViewportClient* ActiveViewport = Editor.GetActiveViewport();
	ImGui::BeginDisabled(!ActiveViewport || ActiveViewport->GetWorldType() != EWorldType::Editor);
	ShowGizmoSettings(Editor);
	ImGui::EndDisabled();
	ImGui::End();
}

void FImguiPropertyWindow::ShowActorHeader(const AActor& Actor) const
{
	const char* ActorClassName = Actor.GetClass() ? Actor.GetClass()->GetDisplayName().c_str() : "None";
	ImGui::Text("Actor Class: %s", ActorClassName);
	ImGui::Text("Actor UUID: %u", Actor.GetUUID());
}

void FImguiPropertyWindow::ShowComponentHierarchy(FEditor& Editor, AActor& Actor)
{
	ImGui::PushID(&Actor);
	ShowAddComponentMenu(Editor, Actor);
	ImGui::Separator();
	ImGui::TextDisabled("Owned Components");

	if (ImGui::Selectable("Actor", Editor.ActorSelected()))
		Editor.SelectActor(Editor.GetSelectedActor());

	for (UActorComponent* Comp : Actor.GetAttachedComponents())
	{
		if (!Comp) continue;

		const char* Name = Comp->GetClass()
			? Comp->GetClass()->GetDisplayName().c_str() : "Component";
		const std::string Label =
			(Comp == Actor.GetRootComponent() ? std::string("[Root] ") : std::string()) +
			Name + " (ID: " + std::to_string(Comp->GetUUID()) + ")";

		// 같은 클래스의 Component가 여러 개 있어도 위젯 ID가 겹치지 않는다.
		ImGui::PushID(Comp);
		if (ImGui::Selectable(Label.c_str(), Editor.GetSelectedComponent() == Comp))
			Editor.SelectComponent(Comp);

		// 기존 Component PushID 범위 안에서 목록 항목의 우클릭 메뉴를 연다.
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Component", nullptr, false, Editor.CanEditActorProperties(&Actor)))
			{
				// Owned Components 순회와 ImGui 스택 처리가 끝난 뒤 삭제한다.
				Editor.RequestComponentDeletion(Comp);
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
	}
	ImGui::PopID();

}

void FImguiPropertyWindow::ShowComponentSections(FEditor& Editor, AActor& Actor)
{
	USceneComponent* RootComp = Actor.GetRootComponent();

	for (UActorComponent* Comp : Actor.GetAttachedComponents())
	{
		if (!Comp)
		{
			continue;
		}

		const bool bIsRoot = (Comp == RootComp);
		const char* CompTypeName = Comp->GetClass() ? Comp->GetClass()->GetDisplayName().c_str() : "Component";

		// ### 뒤쪽이 실제 ID 라서, 앞의 표시 이름이 바뀌어도 접힘 상태가 유지된다.
		std::string SectionTitle = (bIsRoot ? std::string("[Root] ") : std::string()) + CompTypeName + 
			" (ID: " + std::to_string(Comp->GetUUID()) + ")###CompHeader_" + std::to_string(Comp->GetUUID());
		
		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen;
		if (Editor.GetSelectedComponent() == Comp)
			Flags |= ImGuiTreeNodeFlags_Selected;

		// 아웃라이너·목록에서 선택하면 해당 섹션을 펼치고 그 위치로 이동한다.
		const bool bFocus = Editor.ConsumeComponentFocusRequest(Comp);
		if (bFocus) ImGui::SetNextItemOpen(true, ImGuiCond_Always);

		const bool bOpen = ImGui::CollapsingHeader(SectionTitle.c_str(), Flags);
		if (bFocus) ImGui::SetScrollHereY(0.0f);
		if (!bOpen) continue;

		ImGui::PushID(Comp);
		ImGui::BeginDisabled(!Editor.CanEditActorProperties(&Actor));
		ShowComponentDetails(Editor, Actor, *Comp, bIsRoot);
		ImGui::EndDisabled();
		ImGui::PopID();
		ImGui::Spacing();
	}
}

void FImguiPropertyWindow::ShowComponentDetails(FEditor& Editor, AActor& Actor,
	UActorComponent& Comp, bool bIsRoot)
{
	if (USceneComponent* SceneComp = Comp.Cast<USceneComponent>())
	{
		ShowTransform(Editor, *SceneComp);
	}
	if (UMovementComponent* Movement = Comp.Cast<UMovementComponent>())
	{
		ShowMovementSettings(Actor, *Movement);
		if (URotationMovementComponent* Rotating = Comp.Cast<URotationMovementComponent>())
			ShowRotationMovementSettings(*Rotating);
		else if (UProjectileMovementComponent* Projectile = Comp.Cast<UProjectileMovementComponent>())
			ShowProjectileMovementSettings(*Projectile);
		return;

	}
	if (Comp.IsA<UTextInstanceComponent>())
	{
		ShowTextSettings(static_cast<UTextInstanceComponent&>(Comp));
	}
	else if (Comp.IsA<UAnimatedBillboardComp>())
	{
		auto& BillboardComp = static_cast<UAnimatedBillboardComp&>(Comp);
		ShowBillboardSettings(BillboardComp);
		ShowAnimatedBillboardSettings(BillboardComp);
	}
	else if (Comp.IsA<UBillBoardComp>())
	{
		ShowBillboardSettings(static_cast<UBillBoardComp&>(Comp));
	}
	else if (Comp.IsA<USpotLightComponent>())
	{
		ShowSpotLightSettings(static_cast<USpotLightComponent&>(Comp));
	}
	else if (Comp.IsA<UPointLightComponent>())
	{
		ShowPointLightSettings(static_cast<UPointLightComponent&>(Comp));
	}

	else if (Comp.IsA<UStaticMeshComponent>())
	{
		ShowStaticMeshSettings(Actor, static_cast<UStaticMeshComponent&>(Comp), bIsRoot);
	}
}

void FImguiPropertyWindow::ShowTransform(FEditor& Editor, USceneComponent& Comp) const
{
	ImGui::TextDisabled("Relative Transform");

	FTransform Relative = Comp.GetRelativeTransform();
	bool bChanged = false;

	FVector Location = Relative.GetLocation();
	if (ImGui::DragFloat3("Rel Location", &Location.X, 0.01f))
	{
		Relative.SetLocation(Location);
		bChanged = true;
	}
	FVector Euler = Relative.GetRotation().ToEulerXYZDeg();
	if (ImGui::DragFloat3("Rel Rotation (deg)", &Euler.X, 0.5f))
	{
		Relative.SetRotation(FQuaternion::FromEulerXYZDeg(Euler));
		bChanged = true;
	}
	FVector Scale = Relative.GetScale3D();
	if (ImGui::DragFloat3("Rel Scale", &Scale.X, 0.01f))
	{
		Relative.SetScale3D(Scale);
		bChanged = true;
	}
	if (bChanged)
	{
		// 기존 Setter가 자손까지 Dirty 처리한다.
		Comp.SetRelativeTransform(Relative);

		// 선택 대상의 부모를 편집한 경우에도 기즈모 월드 위치를 함께 갱신한다.
		Editor.RefreshSelectedTransform();
	}

}

void FImguiPropertyWindow::ShowTextSettings(UTextInstanceComponent& TextComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Text Settings");


	ImGui::TextDisabled("Font");
	const UFont* Font = TextComp.GetFont();
	const FString FontLabel = Font ? Font->GetID().ToString() : "No Font";
	const float FullWidth = ImGui::GetContentRegionAvail().x;
	ImGui::Button(FontLabel.c_str(), ImVec2(FullWidth, SlotSize));

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
		{
			const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);
			if (Dropped && Dropped->Ptr)
			{
				if (UFont* NewFont = Dropped->Ptr->Cast<UFont>())
				{
					TextComp.SetFont(NewFont);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	static char utfBuffer[512]{};
	WideCharToMultiByte(CP_UTF8, 0, TextComp.GetText().c_str(), -1, &utfBuffer[0], sizeof(utfBuffer), NULL, NULL);

	if (ImGui::InputText("Text Content", &utfBuffer[0], sizeof(utfBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		FString Buffer{ &utfBuffer[0] };
		uint32 convertResult = MultiByteToWideChar(CP_UTF8, 0, Buffer.c_str(), Buffer.length(), NULL, 0);
		FWString newText(convertResult, 0);
		MultiByteToWideChar(CP_UTF8, 0, Buffer.c_str(), Buffer.length(), newText.data(), convertResult);
		TextComp.SetText(newText);
	}

	ImGui::TextDisabled("Text Bounds");
	ImGui::Text("Width: %.2f", TextComp.GetWidth());
	ImGui::Text("Height: %.2f", TextComp.GetHeight());
}

void FImguiPropertyWindow::ShowBillboardSettings(UBillBoardComp& BillboardComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Billboard Settings");
	
	const TMap<FName, TSharedPtr<FTexture>> AllTextures = FRenderResourceLibrary::Get().GetAllTextures();
	UTexture* TextureAsset = BillboardComp.GetTexture();
	FTexture* Texture = TextureAsset ? TextureAsset->Get() : nullptr;
	FName SelectedSpriteName = TextureAsset ? TextureAsset->GetName() : "";
	const float FullWidth = ImGui::GetContentRegionAvail().x;

	ImGui::TextDisabled("Texture");
	if (Texture && Texture->GetSRV())
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(Texture->GetSRV()), ImVec2(SlotSize, SlotSize));
	}
	else
	{
		ImGui::Button("No\nTexture", ImVec2(FullWidth, SlotSize));
	}

	ImGui::SameLine(0.0f, 10.0f);
	
	if (ImGui::BeginCombo("##Sprite", SelectedSpriteName.ToString().c_str()))
	{
		for (const auto& Item : AllTextures)
		{
			UTexture* Texture = FAssetRegistry::GetInstance().Get<UTexture>(Item.first);
			const bool bIsSelected = (SelectedSpriteName == Texture->GetName());
			FString ItemDisplayName = Texture->GetName().ToString();

			if (ImGui::Selectable(ItemDisplayName.c_str(), bIsSelected))
			{
				SelectedSpriteName = ItemDisplayName;
				BillboardComp.SetTexture(Texture);
			}

			if (bIsSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
		{
			const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);
			if (Dropped && Dropped->Ptr)
			{
				if (UTexture* NewTexture = Dropped->Ptr->Cast<UTexture>())
				{
					BillboardComp.SetTexture(NewTexture);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	FVector2 UVScale = BillboardComp.GetUVScale();
	if (ImGui::DragFloat2("UV Scale", &UVScale.X, 0.01f))
	{
		BillboardComp.SetUVScale(UVScale);
	}

	FVector2 UVOffset = BillboardComp.GetUVOffset();
	if (ImGui::DragFloat2("UV Offset", &UVOffset.X, 0.01f))
	{
		BillboardComp.SetUVOffset(UVOffset);
	}
}

void FImguiPropertyWindow::ShowAnimatedBillboardSettings(UAnimatedBillboardComp& BillboardComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.25f, 1.0f), "Animation Settings");

	int GridX = BillboardComp.GetGridX();
	int GridY = BillboardComp.GetGridY();
	int TotalFrames = BillboardComp.GetTotalFrames();
	bool bSpriteSheetChanged = ImGui::DragInt("Grid X", &GridX, 1.0f, 1, 256);
	bSpriteSheetChanged |= ImGui::DragInt("Grid Y", &GridY, 1.0f, 1, 256);
	bSpriteSheetChanged |= ImGui::DragInt("Total Frames", &TotalFrames, 1.0f, 1, 65536);
	if (bSpriteSheetChanged)
	{
		GridX = std::max(GridX, 1);
		GridY = std::max(GridY, 1);
		TotalFrames = std::clamp(TotalFrames, 1, GridX * GridY);
		BillboardComp.SetSpriteSheet(GridX, GridY, BillboardComp.GetFrameRate(), TotalFrames);
	}

	float FrameRate = BillboardComp.GetFrameRate();
	if (ImGui::DragFloat("Frame Rate", &FrameRate, 0.1f, 0.1f, 240.0f))
	{
		BillboardComp.SetFrameRate(std::max(FrameRate, 0.1f));
	}

	int CurrentFrame = BillboardComp.GetCurrentFrame();
	if (ImGui::SliderInt("Current Frame", &CurrentFrame, 0, std::max(0, BillboardComp.GetTotalFrames() - 1)))
	{
		BillboardComp.SetCurrentFrame(CurrentFrame);
	}

	bool bLoop = BillboardComp.IsLooping();
	if (ImGui::Checkbox("Loop", &bLoop))
	{
		BillboardComp.SetLooping(bLoop);
	}

	if (BillboardComp.IsPlaying())
	{
		if (ImGui::Button("Pause")) { BillboardComp.Pause(); }
	}
	else
	{
		if (ImGui::Button("Play")) { BillboardComp.Play(); }
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop")) { BillboardComp.Stop(); }
}

void FImguiPropertyWindow::ShowSpotLightSettings(USpotLightComponent& LightComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Spot Light Settings");

	FVector LightCol = LightComp.GetLightColor();
	if (ImGui::ColorEdit3("Light Color", &LightCol.X))
	{
		LightComp.SetLightColor(LightCol);
	}

	float LightIntensity = LightComp.GetIntensity();
	if (ImGui::DragFloat("Intensity", &LightIntensity, 0.05f, 0.0f, 50.0f))
	{
		LightComp.SetIntensity(LightIntensity);
	}

	float SpotAngle = LightComp.GetSpotAngle();
	if (ImGui::SliderFloat("Spot Angle", &SpotAngle, 1.0f, 89.0f))
	{
		LightComp.SetSpotAngle(SpotAngle);
	}

	float LightRange = LightComp.GetRange();
	if (ImGui::DragFloat("Range", &LightRange, 0.1f, 0.1f, 100.0f))
	{
		LightComp.SetRange(LightRange);
	}
}

void FImguiPropertyWindow::ShowPointLightSettings(UPointLightComponent& LightComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Point Light Settings");

	FVector4 LightCol = LightComp.GetLightColor();
	if (ImGui::ColorEdit3("Light Color", &LightCol.X))
	{
		LightComp.SetLightColor(LightCol);
	}

	float LightIntensity = LightComp.GetIntensity();
	if (ImGui::DragFloat("Intensity", &LightIntensity, 0.05f, 0.0f, 50.0f))
	{
		LightComp.SetIntensity(LightIntensity);
	}

	float LightRadius = LightComp.GetAttenuationRadius();
	if (ImGui::DragFloat("Attenuation Radius", &LightRadius, 0.1f, 0.1f, 100.0f))
	{
		LightComp.SetAttenuationRadius(LightRadius);
	}
}

void FImguiPropertyWindow::ShowStaticMeshSettings(AActor& Actor, UStaticMeshComponent& MeshComp, bool bIsRoot) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Static Mesh Settings");


	FAssetRegistry& Registry = FAssetRegistry::GetInstance();

	if (ImGui::BeginTable(
		"StaticMeshAssetSlots",
		3,
		ImGuiTableFlags_SizingStretchSame
	))
	{
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ShowStaticMeshSlot(MeshComp);

		if (MeshComp.GetMaterialSlotLength() > 1)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ShowApplyAllMaterialSlot(MeshComp);

			bool bAllSlotsHaveMaterial = true;
			for (int i = 0; i < MeshComp.GetMaterialSlotLength(); ++i)
			{
				const FMaterialInstance* Instance = MeshComp.GetMaterialInstance(i);
				if (!Instance || !Instance->Material)
				{
					bAllSlotsHaveMaterial = false;
					break;
				}
			}

			if (bAllSlotsHaveMaterial)
			{
				ImGui::TableSetColumnIndex(1);
				ShowApplyAllTextureSlot(MeshComp);

				ImGui::TableSetColumnIndex(2);
				ShowApplyAllPipelineSlot(MeshComp);
			}
		}

		for (int i = 0; i < MeshComp.GetMaterialSlotLength(); ++i)
		{
			ImGui::PushID(i);

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ShowMaterialSlot(MeshComp, i);

			const FMaterialInstance* Instance = MeshComp.GetMaterialInstance(i);
			if (Instance && Instance->Material)
			{
				ImGui::TableSetColumnIndex(1);
				ShowTextureSlot(MeshComp, i);

				ImGui::TableSetColumnIndex(2);
				ShowPipelineSlot(MeshComp, i);
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FImguiPropertyWindow::ShowMaterialSlot(UStaticMeshComponent& MeshComp, int Slot) const
{
	const FMaterialInstance* Instance = MeshComp.GetMaterialInstance(Slot);
	UMaterial* Material = Instance ? Instance->Material : nullptr;

	ImGui::Spacing();
	ImGui::TextDisabled("Material");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	const FString Label = Material ? Material->GetID().ToString() : "No Material";
	ImGui::Button(Label.c_str(), ImVec2(FullWidth, SlotSize));

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UMaterial* NewMaterial = Dropped->Ptr->Cast<UMaterial>();

			if (NewMaterial)
			{
				MeshComp.SetMaterial(NewMaterial, Slot);
			}
		}
	}

	ImGui::EndDragDropTarget();
}

void FImguiPropertyWindow::ShowPipelineSlot(UStaticMeshComponent& MeshComp, int Slot) const
{
	UPipeline* Pipeline = MeshComp.GetMaterialInstance(Slot)->Pipeline;

	ImGui::Spacing();
	ImGui::TextDisabled("Pipeline");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	ImGui::Button(Pipeline->GetID().ToString().c_str(), ImVec2(FullWidth, SlotSize));

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UPipeline* NewPipeline = Dropped->Ptr->Cast<UPipeline>();

			if (NewPipeline)
			{
				MeshComp.SetPipeline(NewPipeline, Slot);
			}
		}
	}

	ImGui::EndDragDropTarget();
}

void FImguiPropertyWindow::ShowTextureSlot(UStaticMeshComponent& MeshComp, int Slot) const
{
	UTexture* TextureAsset = MeshComp.GetMaterialInstance(Slot)->Texture;
	FTexture* CurrentTexture = nullptr;
	
	if (TextureAsset)
	{
		CurrentTexture = TextureAsset->Get();
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Texture");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	if (CurrentTexture && CurrentTexture->GetSRV())
	{
		const ImTextureID TexId = reinterpret_cast<ImTextureID>(CurrentTexture->GetSRV());
		ImGui::Image(TexId, ImVec2(FullWidth, SlotSize));
	}
	else
	{
		ImGui::Button("No\nTexture", ImVec2(FullWidth, SlotSize));
	}

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UTexture* Texture = Dropped->Ptr->Cast<UTexture>();

			if (Texture)
			{
				MeshComp.SetTexture(Texture, Slot);
			}
		}
	}

	ImGui::EndDragDropTarget();
}

void FImguiPropertyWindow::ShowStaticMeshSlot(UStaticMeshComponent& MeshComp) const
{
	const UStaticMesh* StaticMesh = MeshComp.GetMesh();

	ImGui::Spacing();
	ImGui::TextDisabled("StaticMesh");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	const FString MeshLabel = StaticMesh ? StaticMesh->GetID().ToString() : "None";
	ImGui::Button(MeshLabel.c_str(), ImVec2(FullWidth, SlotSize));

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UStaticMesh* NewStaticMesh = Dropped->Ptr->Cast<UStaticMesh>();

			if (NewStaticMesh)
			{
				MeshComp.SetMesh(NewStaticMesh);
			}
		}
	}

	ImGui::EndDragDropTarget();
}


void FImguiPropertyWindow::ShowApplyAllMaterialSlot(UStaticMeshComponent& MeshComp) const
{
	ImGui::Spacing();
	ImGui::TextDisabled("Material");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	ImGui::Button("Apply All Material", ImVec2(FullWidth, SlotSize));

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UMaterial* NewMaterial = Dropped->Ptr->Cast<UMaterial>();

			if (NewMaterial)
			{
				for (int i = 0; i < MeshComp.GetMaterialSlotLength(); ++i)
				{
					MeshComp.SetMaterial(NewMaterial, i);
				}
			}
		}
	}

	ImGui::EndDragDropTarget();
}

void FImguiPropertyWindow::ShowApplyAllPipelineSlot(UStaticMeshComponent& MeshComp) const
{
	ImGui::Spacing();
	ImGui::TextDisabled("Pipeline");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	ImGui::Button("Apply All Pipeline", ImVec2(FullWidth, SlotSize));

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UPipeline* NewPipeline = Dropped->Ptr->Cast<UPipeline>();

			if (NewPipeline)
			{
				for (int i = 0; i < MeshComp.GetMaterialSlotLength(); ++i)
				{
					MeshComp.SetPipeline(NewPipeline, i);
				}
			}
		}
	}

	ImGui::EndDragDropTarget();
}

void FImguiPropertyWindow::ShowApplyAllTextureSlot(UStaticMeshComponent& MeshComp) const
{
	ImGui::Spacing();
	ImGui::TextDisabled("Texture");

	// 슬롯 만들기
	float FullWidth = ImGui::GetContentRegionAvail().x;
	ImGui::Button("Apply All Texture", ImVec2(FullWidth, SlotSize));

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget()) { return; }

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

		if (Dropped->Ptr)
		{
			UTexture* Texture = Dropped->Ptr->Cast<UTexture>();

			if (Texture)
			{
				for (int i = 0; i < MeshComp.GetMaterialSlotLength(); ++i)
				{
					MeshComp.SetTexture(Texture, i);
				}
			}
		}
	}

	ImGui::EndDragDropTarget();
}


void FImguiPropertyWindow::ShowGizmoSettings(FEditor& Editor) const
{
	static const char* GizmoModes[4] = { "None", "Translation", "Rotation", "Scale" };
	int SelectedItem = static_cast<int>(Editor.GetGizmo().Mode);
	if (ImGui::Combo("Gizmo Mode", &SelectedItem, GizmoModes, 4))
	{
		Editor.GetGizmo().Mode = static_cast<EGizmoMode>(SelectedItem);
	}

	if (SelectedItem == 3) // Scale
	{
		static const char* GizmoSpaces[] = { "Local" };
		SelectedItem = static_cast<int>(Editor.GetGizmo().GetSpace()) - 1;
		if (ImGui::Combo("Gizmo Space", &SelectedItem, GizmoSpaces, 1))
		{
			Editor.GetGizmo().SetGizmoSpace(static_cast<EGizmoSpace>(SelectedItem - 1));
		}
	}
	else if (SelectedItem != 0) // Translation, Rotation
	{
		static const char* GizmoSpaces[] = { "World", "Local" };
		SelectedItem = static_cast<int>(Editor.GetGizmo().GetSpace());
		if (ImGui::Combo("Gizmo Space", &SelectedItem, GizmoSpaces, 2))
		{
			Editor.GetGizmo().SetGizmoSpace(static_cast<EGizmoSpace>(SelectedItem));
		}
	}
}

void FImguiPropertyWindow::ShowAddComponentMenu(FEditor& Editor, AActor& Actor)
{
	// 현재 World의 Actor라면 Editor와 PIE 모두 추가를 허용한다.
	ImGui::BeginDisabled(!Editor.CanEditActorProperties(&Actor));

	// 버튼 폭은 표시 문자열과 ImGui 스타일로 계산한다.
	// 항목이 많아지면 드롭리스트 내부에 스크롤이 생긴다.
	if (ImGui::BeginCombo("##AddComponentCombo", "+ Add Component", ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_HeightLarge))
	{
		// 목록을 새로 열 때 검색을 초기화하고 입력란에 포커스를 준다.
		if (ImGui::IsWindowAppearing())
		{
			ComponentFilter.Clear();
			ImGui::SetKeyboardFocusHere();
		}

		ComponentFilter.Draw("Search");
		ImGui::Separator();

		bool bHasMatch = false;
		for (uint32 Index = 0; Index < UClass::GetRegisteredClassCount(); ++Index)
		{
			UClass* ClassType = UClass::GetClassById(Index);
			if (!FEditor::IsAddableComponentClass(ClassType)) continue;

			const FString& Name = ClassType->GetDisplayName();
			if (!ComponentFilter.PassFilter(Name.c_str())) continue;
			bHasMatch = true;

			// 표시 이름이 같더라도 클래스별 위젯 ID를 구분한다.
			ImGui::PushID(ClassType);
			bool bAdded = false;

			// 추가에 성공했을 때 직접 닫도록 자동 닫기를 해제한다.
			if (ImGui::Selectable(Name.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups))
			{
				// 선택한 SceneComponent 아래에 추가한다.
				// 부모가 nullptr이면 공통 생성 함수가 Root 아래로 처리한다.
				bAdded = Editor.AddComponentToActor(&Actor, ClassType, Editor.GetTransformTarget()) != nullptr;
			}

			ImGui::PopID();

			if (bAdded)
			{
				ImGui::CloseCurrentPopup();
				break;
			}
		}

		if (!bHasMatch) ImGui::TextDisabled("No matching components");
		ImGui::EndCombo();
	}

	ImGui::EndDisabled();
}

void FImguiPropertyWindow::ShowMovementSettings(AActor& Actor, UMovementComponent& Movement) const
{
	ImGui::Separator();
	ImGui::TextDisabled("Movement Settings");

	// Tick 활성 상태 변경은 기존 Setter를 통해 World 레지스트리에 반영한다.
	bool bTickEnabled = Movement.IsTickEnabled();
	if (ImGui::Checkbox("Tick Enabled", &bTickEnabled))
		Movement.SetComponentTickEnabled(bTickEnabled);

	ImGui::Checkbox("Tick In Editor", &Movement.bTickInEditor);
	ImGui::Checkbox("Auto Root When Unassigned", &Movement.bAutoRegisterUpdatedComponent);

	// 같은 클래스가 여러 개 있어도 UUID로 이동 대상을 구분한다.
	USceneComponent* Target = Movement.GetUpdatedComponent();
	const FString Preview = Target
		? Target->GetClass()->GetDisplayName()
		+ " (ID: " + std::to_string(Target->GetUUID()) + ")"
		: "None";

	if (ImGui::BeginCombo("Updated Component", Preview.c_str()))
	{
		if (ImGui::Selectable("None", Target == nullptr))
			Movement.SetUpdatedComponent(nullptr);

		// Actor의 전체 소유 목록에서 SceneComponent만 고른다.
		for (UActorComponent* Component : Actor.GetAttachedComponents())
		{
			USceneComponent* Scene = Component ? Component->Cast<USceneComponent>() : nullptr;
			if (!Scene) continue;

			const FString Label =
				(Scene == Actor.GetRootComponent() ? FString("[Root] ") : FString())
				+ Scene->GetClass()->GetDisplayName()
				+ " (ID: " + std::to_string(Scene->GetUUID()) + ")";

			if (ImGui::Selectable(Label.c_str(), Scene == Target))
				Movement.SetUpdatedComponent(Scene);
		}
		ImGui::EndCombo();
	}
}

void FImguiPropertyWindow::ShowRotationMovementSettings(URotationMovementComponent& Movement) const
{
	ImGui::Separator();
	ImGui::TextDisabled("Rotation Settings");

	// XYZ 속도는 도/s 단위다. 체크를 끄면 월드축 기준으로 회전한다.
	ImGui::DragFloat("Roll Rate (X, deg/s)", &Movement.RotationRate.X, 1.0f);
	ImGui::DragFloat("Pitch Rate (Y, deg/s)", &Movement.RotationRate.Y, 1.0f);
	ImGui::DragFloat("Yaw Rate (Z, deg/s)", &Movement.RotationRate.Z, 1.0f);

	ImGui::Checkbox("Rotate In Local Space", &Movement.bRotationInLocalSpace);

	ImGui::DragFloat3("Pivot Offset XYZ", &Movement.PivotTranslation.X, 0.1f);

}

void FImguiPropertyWindow::ShowProjectileMovementSettings(UProjectileMovementComponent& Movement) const
{
	ImGui::Separator();
	ImGui::TextDisabled("Projectile Settings");

	// 방향과 초기 속력은 다음 발사 또는 Apply 버튼을 누를 때 반영한다.
	ImGui::DragFloat3("Launch Direction XYZ", &Movement.LaunchDirection.X, 0.1f);

	if (ImGui::DragFloat("Initial Speed (units/s)", &Movement.InitialSpeed, 0.1f))
		Movement.InitialSpeed = std::max(0.0f, Movement.InitialSpeed);

	if (ImGui::DragFloat("Max Speed (0 = unlimited)", &Movement.MaxSpeed, 0.1f))
		Movement.MaxSpeed = std::max(0.0f, Movement.MaxSpeed);

	ImGui::DragFloat3("Accelaretion", &Movement.Acceleration.X, 0.1f);
	ImGui::Checkbox("Enable Gravity", &Movement.bEnableGravity);
	ImGui::DragFloat3("Gravity Acceleration", &Movement.GravityAcceleration.X, 0.1f);
	ImGui::Checkbox("Initial Velocity In Local Space", &Movement.bInitialVelocityInLocalSpace);

	// 현재 위치를 유지하고 현재 대상의 자세로 발사 설정을 다시 적용한다.
	if (ImGui::Button("Apply Launch Settings"))
		Movement.ResetVelocity();

	const FVector& Velocity = Movement.GetVelocity();
	ImGui::Text("World Velocity: %.3f, %.3f, %.3f", Velocity.X, Velocity.Y, Velocity.Z);
}
