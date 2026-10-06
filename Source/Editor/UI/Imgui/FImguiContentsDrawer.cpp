#include "FImguiContentsDrawer.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Rendering/FTexture.h"
#include "Runtime/Utility/EngineUtil.h"
#include <algorithm>
#include <cctype>
#include "FImguiDragDrop.h"
#include "Runtime/CoreUObject/UClass.h"

namespace fs = std::filesystem;

FImguiContentsDrawer::FImguiContentsDrawer() : LeftPanelWidth(200.0f)
{
	RootPath = "";
	CurrentPath = RootPath;
}

void FImguiContentsDrawer::Process(FEditor& Editor)
{
	if (Editor.bHideUI || Editor.bZenMode)
	{
		return;
	}

	ImGui::Begin("Content Drawer");

	// GetContentRegionAvail은 Begin 다음에 불러야 이 창의 남은 영역이 나온다.
	// Begin 이전에 부르면 직전 창의 값이라 자식 패널이 창 밖으로 삐져나간다.
	ImVec2 ContentSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild("LeftPanel", ImVec2(LeftPanelWidth, ContentSize.y), true);
	RenderFolderTree();
	ImGui::EndChild();

	ImGui::SameLine();

	// 폭 0은 남은 공간을 전부 쓰라는 뜻
	ImGui::BeginChild("RightPanel", ImVec2(0.0f, ContentSize.y), true);
	if (bShowComponents) RenderComponentView();
	else RenderContentView();
	ImGui::EndChild();

	ImGui::End();

}

void FImguiContentsDrawer::RenderContentView()
{
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	FFolderView FolderView = Registry.GetAssetDirectory(CurrentPath);

	// ---------------- 상단 바 ----------------
	// 루트 기준 상대 경로를 보여준다.
	ImGui::TextUnformatted(CurrentPath.string().c_str());

	if (CurrentPath != RootPath)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Up")) { CurrentPath = CurrentPath.parent_path(); }
	}


	ImGui::Separator();


	int Total = FolderView.Folders.size() + FolderView.Assets.size();
	if (Total == 0)
	{
		ImGui::TextDisabled("비어 있습니다.");
		return;
	}

	// ---------------- 타일 그리드 ----------------
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float TileWidth = ThumbnailSize + Style.ItemSpacing.x;
	const float Avail = ImGui::GetContentRegionAvail().x;

	// 패널을 좁히면 0이 되어 나눗셈이 깨지므로 최소 1로 막는다.
	int Columns = std::max(1, static_cast<int>(Avail / TileWidth));

	// 폴더 진입은 순회 중에 CurrentPath를 바꾸면 안 되므로 따로 모아 뒀다가 끝나고 적용한다.
	std::filesystem::path PendingNavigate;

	int Index = 0;

	// GetAssetDirectory가 정렬한 폴더를 먼저, 에셋을 그 다음에 렌더링한다.
	for (const fs::path& Item : FolderView.Folders)
	{
		// 같은 이름이 있어도 ID가 겹치지 않도록 Key값으로 구분
		ImGui::PushID(Item.c_str());
		ImGui::BeginGroup();

		const bool bSelected = (SelectedPath == Item);

		// 폴더는 썸네일이 없으므로 에디터 아이콘으로 대신한다.
		// 아이콘이 없으면 DisplayImage가 nullptr이 되어 아래 else로 떨어진다.
		UTexture* Icon = Registry.Get<UTexture>("Texture/Editor/Icon_Folder.json");
		FTexture* DisplayImage = Icon ? Icon->Get() : nullptr;

		if (DisplayImage && DisplayImage->GetSRV())          
		{
			// 선택 상태를 배경색으로 표시한다.
			if (bSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			}

			// ImGui 1.93의 ImTextureID는 ImU64라서 포인터를 정수로 한 번 거쳐야 한다.
			const ImTextureID TexId = reinterpret_cast<ImTextureID>(DisplayImage->GetSRV());

			if (ImGui::ImageButton("##thumb", TexId, ImVec2(ThumbnailSize, ThumbnailSize)))
			{
				SelectedPath = Item;
			}

			ImGui::PopStyleColor();
		}
		else
		{
			// 이미지가 아니거나 아직 로드 전이면 종류를 글자로 보여준다.
			if (ImGui::Selectable(
				"[DIR]",
				bSelected,
				ImGuiSelectableFlags_AllowDoubleClick,
				ImVec2(ThumbnailSize, ThumbnailSize)
			))
			{
				SelectedPath = Item;
			}
		}

		if (ImGui::IsItemHovered() && !ImGui::IsDragDropActive())
		{
			ImGui::SetTooltip("%s", Item.string().c_str());
		}

		// 더블클릭은 Selectable 반환값이 아니라 항목 위에서 직접 판정한다.
		// 반환값 안에서 보면 클릭 타이밍에 따라 놓치는 경우가 있다.
		if (ImGui::IsItemHovered() &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			PendingNavigate = Item;
		}

		// 이름이 길면 썸네일 폭 안에서 줄바꿈한다.
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ThumbnailSize);
		ImGui::TextUnformatted(Item.string().c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();
		ImGui::PopID();

		// 행의 마지막이 아니면 옆에 붙인다.
		if ((Index + 1) % Columns != 0 && Index + 1 < Total)
		{
			ImGui::SameLine();
		}

		++Index;
	}
	for (UAsset* Item : FolderView.Assets)
	{
		FString Path = Item->GetID().ToString();

		// 같은 이름이 있어도 ID가 겹치지 않도록 Key값으로 구분
		ImGui::PushID(Path.c_str());
		ImGui::BeginGroup();

		const bool bSelected = (SelectedPath == Path);

		// 폴더는 썸네일이 없으므로 에디터 아이콘으로 대신한다.
		// 아이콘이 없으면 DisplayImage가 nullptr이 되어 아래 else로 떨어진다.

		UTexture* Icon = Registry.Get<UTexture>("Texture/Editor/Icon_Folder.json");

		if (Item->IsA<UPipeline>())
		{
			Icon = Registry.Get<UTexture>("Texture/Editor/Icon_Pipeline.json");
		}
		else if (Item->IsA<UMaterial>())
		{
			Icon = Registry.Get<UTexture>("Texture/Editor/Icon_Material.json");
		}
		else if (Item->IsA<UFont>())
		{
			Icon = Registry.Get<UTexture>("Texture/Editor/Icon_Font.json");
		}
		else if (Item->IsA<UStaticMesh>())
		{
			Icon = Registry.Get<UTexture>("Texture/Editor/Icon_StaticMesh.json");
		}
		else if (Item->IsA<UTexture>())
		{
			Icon = Item->Cast<UTexture>();
		}

		FTexture* DisplayImage = Icon ? Icon->Get() : nullptr;

		if (DisplayImage && DisplayImage->GetSRV())          
		{
			// 선택 상태를 배경색으로 표시한다.
			if (bSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			}

			// ImGui 1.93의 ImTextureID는 ImU64라서 포인터를 정수로 한 번 거쳐야 한다.
			const ImTextureID TexId = reinterpret_cast<ImTextureID>(DisplayImage->GetSRV());

			ImGui::ImageButton("##thumb", TexId, ImVec2(ThumbnailSize, ThumbnailSize));
			ImGui::PopStyleColor();
		}
		else
		{
			// 이미지가 아니거나 아직 로드 전이면 종류를 글자로 보여준다.
			ImGui::Selectable(
				"[FILE]",
				bSelected,
				ImGuiSelectableFlags_AllowDoubleClick,
				ImVec2(ThumbnailSize, ThumbnailSize)
			);
		}
		
		if (ImGui::BeginDragDropSource())
		{
			FContentDragPayload DragData;
			DragData.Ptr = Item;

			// ImGui가 내부 버퍼로 복사하므로 지역 변수를 넘겨도 된다.
			ImGui::SetDragDropPayload(ContentDragPayloadType, &DragData, sizeof(DragData));

			ImGui::EndDragDropSource();
		}

		if (ImGui::IsItemHovered() && !ImGui::IsDragDropActive())
		{
			ImGui::SetTooltip("%s", Path.c_str());
		}

		// 이름이 길면 썸네일 폭 안에서 줄바꿈한다.
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ThumbnailSize);
		ImGui::TextUnformatted(Item->GetName().ToString().c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();
		ImGui::PopID();

		// 행의 마지막이 아니면 옆에 붙인다.
		if ((Index + 1) % Columns != 0 && Index + 1 < Total)
		{
			ImGui::SameLine();
		}

		++Index;
	}

	if (!PendingNavigate.empty())
	{
		CurrentPath /= PendingNavigate;
	}
}

void FImguiContentsDrawer::RenderFolderTree()
{
	ImGui::TextUnformatted("Folders");
	ImGui::Separator();

	// 파일 시스템과 별개인 클래스 폴더이므로 하위 폴더와 TreePop이 없다.
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf |
		ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bShowComponents) Flags |= ImGuiTreeNodeFlags_Selected;

	ImGui::TreeNodeEx("Components###ComponentClasses", Flags);
	if (ImGui::IsItemClicked()) bShowComponents = true;

	// 기존 에셋 폴더 트리는 유지한다.
	RenderFolderTreeNode(RootPath);

}

void FImguiContentsDrawer::RenderFolderTreeNode(const fs::path& FolderPath)
{
	FString FolderName = (FolderPath == RootPath) ? "All" : FolderPath.filename().string();

	// 하위 폴더가 있는지 먼저 확인한다.
	// 없으면 잎 노드로 만들어 열리지 않는 화살표가 생기지 않게 한다.

	FAssetRegistry& Registry = FAssetRegistry::GetInstance();

	FFolderView FolderView = Registry.GetAssetDirectory(FolderPath);

	bool bContainsDirectory = !FolderView.Folders.empty();

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (!bContainsDirectory)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// 선택 여부는 Selected 표시만 결정한다.
	if (!bShowComponents && CurrentPath == FolderPath)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (FolderPath == RootPath)
	{
		Flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	// 이름이 같은 폴더가 여러 곳에 있을 수 있으므로 전체 경로를 ID로 쓴다.
	ImGui::PushID(FolderPath.string().c_str());

	const bool bOpened = ImGui::TreeNodeEx(FolderName.c_str(), Flags);

	// 화살표를 눌러 접고 펴는 것과 폴더를 선택하는 것을 구분한다.
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		bShowComponents = false;
		CurrentPath = FolderPath;
	}

	if (bOpened && bContainsDirectory)
	{
		for (const fs::path& Entry : FolderView.Folders)
		{
			RenderFolderTreeNode(FolderPath / Entry);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}

void FImguiContentsDrawer::RenderComponentView()
{
	ImGui::TextUnformatted("Components");
	ImGui::SameLine();
	if (ImGui::SmallButton("Back to Assets"))
	{
		// 가상 폴더 진입 전의 에셋 경로로 돌아간다.
		bShowComponents = false;
		return;
	}
	ImGui::Separator();
	ImGui::TextDisabled("Drag a class onto the Outliner");

	for (uint32 Index = 0; Index < UClass::GetRegisteredClassCount(); ++Index)
	{
		UClass* ClassType = UClass::GetClassById(Index);
		if (!FEditor::IsAddableComponentClass(ClassType)) continue;

		const FString& Name = ClassType->GetDisplayName();
		ImGui::PushID(ClassType);

		// 이 행은 생성할 클래스다. 클릭만으로 객체를 생성하지 않는다.
		ImGui::Selectable(Name.c_str());

		if (ImGui::BeginDragDropSource())
		{
			// ImGui가 포인터 값을 복사한다. 클래스 객체는 기존 레지스트리에 유지된다.
			ImGui::SetDragDropPayload(
				ComponentClassDragPayloadType, &ClassType, sizeof(ClassType));
			ImGui::Text("Add %s", Name.c_str());
			ImGui::EndDragDropSource();
		}

		ImGui::PopID();
	}
}
