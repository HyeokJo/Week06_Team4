#include "FImguiWorldOutliner.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/UWorld.h"
#include "ThirdParty/Imgui/imgui.h"
#include <string>
#include <algorithm>
#include "FImguiDragDrop.h"

void FImguiWorldOutliner::Process(FEditor& Editor)
{
	if (Editor.bHideUI || Editor.bZenMode)
	{
		return;
	}

	ImGui::Begin("World Outliner");

	UWorld* World = Editor.GetCurrentWorld();
	if (!World)
	{
		ImGui::TextDisabled("No Active Scene");
		ImGui::End();
		return;
	}

	ImGui::Checkbox("아웃라이너 최적화 적용", &bUseOptimized);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("체크: 캐쉬된 라벨 및 화면에 보이는 일부 노드만 랜더\n"
			"해제: 매 프레임 동적 생성 및 전체 순회");
	}
	ImGui::Separator();

	// 검색 필터 버퍼
	const bool bFilterChanged = ShowSearchBar();
	ImGui::Separator();

	auto& Actors = World->GetActors();
	AActor* SelectedActor = Editor.GetSelectedActor();
	// 액터 목록 표시
	ImGui::BeginChild("ActorList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), false);

	// 같은 개수의 Actor가 교체된 경우에도 삭제된 포인터를 캐시에 남기지 않는다.
	bool bActorsChanged = World != LastWorld || Actors.size() != LastActorCount;
	size_t CachedIndex = 0;
	for (AActor* Actor : Actors)
	{
		if (!Actor) continue;
		if (CachedIndex >= CachedActors.size() || CachedActors[CachedIndex].Actor != Actor
			|| CachedActors[CachedIndex].UUID != Actor->GetUUID())
		{
			bActorsChanged = true;
			break;
		}
		++CachedIndex;
	}
	bActorsChanged |= CachedIndex != CachedActors.size();

	if (World != LastWorld)
	{
		ExpandedActorUUIDs.clear();
		ExpandedComponentUUIDs.clear();
	}
	if (!bUseOptimized || bCacheDirty || bActorsChanged)
	{
		RefreshCache(World);
		UpdateFilter(CurrentFilterStr);
		LastWorld = World;
	}
	else if (bFilterChanged)
	{
		UpdateFilter(CurrentFilterStr);
	}

	// 소유/부착 관계는 Actor 개수가 그대로여도 바뀔 수 있다.
	RebuildDisplayList();
	bDisplayListDirty = false;
	if (bUseOptimized)
	{
		ImGuiListClipper Clipper;
		Clipper.Begin(static_cast<int>(DisplayList.size()));
		while (Clipper.Step())
		{
			for (int i = Clipper.DisplayStart; i < Clipper.DisplayEnd; ++i)
				ShowActorNode_Cached(Editor, DisplayList[i], SelectedActor);
		}
	}
	else
	{
		for (const FOutlinerItem& Item : DisplayList)
			ShowActorNode_Cached(Editor, Item, SelectedActor);
	}
	ImGui::EndChild();

	ImGui::Separator();

	// 하단 컨트롤 영역
	if (AActor* Actor = Editor.GetSelectedActor())
	{
		ImGui::BeginDisabled(!Editor.ObjectSelected() || !Editor.CanEditActorProperties(Actor));

		const char* Label = Editor.GetSelectedComponent() ? "Delete Component" : "Delete Actor";
		if (ImGui::Button(Label))
		{
			if (UActorComponent* Component = Editor.GetSelectedComponent())
				Editor.RequestComponentDeletion(Component);
			else if (Editor.DeleteSelectedActor())
				bCacheDirty = true;
		}

		ImGui::EndDisabled();
	}
	else
	{
		ImGui::TextDisabled("No Selection");
	}

	ImGui::End();
}

void FImguiWorldOutliner::RefreshCache(UWorld* World)
{
	CachedActors.clear();
	const auto& Actors = World->GetActors();
	CachedActors.reserve(Actors.size());

	for (AActor* Actor : Actors)
	{
		if (!Actor) { continue; }

		FOutlinerItem Item;
		Item.Type = EOutlinerItemRowType::Actor;
		Item.Actor = Actor;
		Item.UUID = Actor->GetUUID();

		const char* ClassName = Actor->GetClass() ? Actor->GetClass()->GetDisplayName().c_str() : "Actor";
		Item.DisplayLabel = FString(ClassName) + " (ID: " + std::to_string(Item.UUID) + ")";

		Item.LowerLabel = Item.DisplayLabel;
		std::transform(Item.LowerLabel.begin(), Item.LowerLabel.end(), Item.LowerLabel.begin(),
			[](unsigned char c) { return static_cast<char>(::tolower(c)); });

		CachedActors.push_back(std::move(Item));
	}

	LastActorCount = Actors.size();
	bCacheDirty = false;
}

void FImguiWorldOutliner::UpdateFilter(const FString& FilterStr)
{
	FilteredIndices.clear();
	FilteredIndices.reserve(CachedActors.size());

	const bool bHasFilter = !FilterStr.empty();

	for (int32 i = 0; i < static_cast<int32>(CachedActors.size()); ++i)
	{
		if (!bHasFilter || CachedActors[i].LowerLabel.find(FilterStr) != FString::npos)
		{
			FilteredIndices.push_back(i);
		}
	}

	LastFilterStr = FilterStr;
}

void FImguiWorldOutliner::ShowActorNode_Cached(FEditor& Editor, const FOutlinerItem& Item, AActor* SelectedActor)
{
	const float Indent = Item.Depth * 16.0f;
	if (Indent > 0.0f) ImGui::Indent(Indent);

	auto& Expanded = Item.Type == EOutlinerItemRowType::Actor ? ExpandedActorUUIDs : ExpandedComponentUUIDs;
	const bool bIsOpen = Expanded.contains(Item.UUID);

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth |
		ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (!Item.bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf;

	// Component 선택 상태에서는 소유 Actor 행 대신 Component 행을 강조한다.
	const bool bSelected = Item.Type == EOutlinerItemRowType::Actor
		? Editor.ActorSelected() && Item.Actor == SelectedActor
		: Item.Component == Editor.GetSelectedComponent();
	if (bSelected) Flags |= ImGuiTreeNodeFlags_Selected;

	// Clipper가 부모 행을 건너뛸 수 있으므로 기존 펼침 상태 관리 방식을 유지한다.
	ImGui::SetNextItemOpen(bIsOpen, ImGuiCond_Always);
	const bool bNowOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(Item.UUID)),	Flags, "%s", Item.DisplayLabel.c_str());

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		// Actor 행과 Component 행의 선택 경로를 구분한다.
		if (Item.Type == EOutlinerItemRowType::Actor)
			Editor.SelectActor(Item.Actor);
		else
			Editor.SelectComponent(Item.Component);
	}

	if (Item.Type == EOutlinerItemRowType::Component && ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Delete Component", nullptr, false,
			Editor.CanEditActorProperties(Item.Actor)))
		{
			// DisplayList 순회 중에는 삭제하지 않는다.
			Editor.RequestComponentDeletion(Item.Component);
		}
		ImGui::EndPopup();
	}

	// Actor 행은 Root 아래, SceneComponent 행은 해당 컴포넌트 아래를 대상으로 한다.
	const bool bActorRow = Item.Type == EOutlinerItemRowType::Actor;
	USceneComponent* AttachParent = bActorRow ? Item.Actor->GetRootComponent() : Item.Component->Cast<USceneComponent>();

	// 일반 ActorComponent 행에는 Transform 부착 부모가 없으므로 드롭 타깃으로 사용하지 않는다.
	if ((bActorRow || AttachParent != nullptr) && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ComponentClassDragPayloadType))
		{
			// 내부 클래스 드래그에서 전달한 UClass* 값을 읽는다.
			UClass* ClassType = *static_cast<UClass* const*>(Payload->Data);

			if (UActorComponent* Added = Editor.AddComponentToActor(Item.Actor, ClassType, AttachParent))
			{
				// 추가 결과가 다음 프레임의 트리에 나타나도록 대상 행을 펼친다.
				ExpandedActorUUIDs.insert(Item.Actor->GetUUID());
				if (AttachParent && Added->Cast<USceneComponent>())
					ExpandedComponentUUIDs.insert(AttachParent->GetUUID());

				bDisplayListDirty = true;
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (Item.bHasChildren && bNowOpen != bIsOpen)
	{
		if (bNowOpen) Expanded.insert(Item.UUID);
		else Expanded.erase(Item.UUID);
		bDisplayListDirty = true;
	}

	if (Indent > 0.0f) ImGui::Unindent(Indent);
}

bool FImguiWorldOutliner::ShowSearchBar()
{
	// 입력이 변경된 경우에만 검색 필터를 갱신한다.
	ImGui::SetNextItemWidth(-1.0f);
	if (!ImGui::InputTextWithHint(
		"##OutlinerFilter", "Search...", FilterBuffer, sizeof(FilterBuffer)))
	{
		return false;
	}

	// 기존 소문자 라벨과 비교할 수 있도록 검색어도 소문자로 변환한다.
	CurrentFilterStr = FilterBuffer;
	std::transform(
		CurrentFilterStr.begin(), CurrentFilterStr.end(), CurrentFilterStr.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	return true;
}

void FImguiWorldOutliner::RebuildDisplayList()
{
	DisplayList.clear();
	for (int32 ItemIndex : FilteredIndices)
	{
		const FOutlinerItem& ActorItem = CachedActors[ItemIndex];
		AActor* Actor = ActorItem.Actor;
		if (!Actor) continue;

		const auto& Components = Actor->GetAttachedComponents();
		TSet<const UActorComponent*> Owned;
		for (UActorComponent* Component : Components)
			if (Component) Owned.insert(Component);

		FOutlinerItem Row = ActorItem;
		Row.Depth = 0;
		Row.bHasChildren = !Owned.empty();
		DisplayList.push_back(Row);
		if (!ExpandedActorUUIDs.contains(ActorItem.UUID)) continue;

		TSet<const UActorComponent*> Visited;
		auto AppendComponent = [&](auto&& Self, UActorComponent* Component, int32 Depth) -> void
		{
			if (!Component || !Owned.contains(Component) || !Visited.insert(Component).second)
				return;

			USceneComponent* SceneComponent = Component->Cast<USceneComponent>();
			bool bHasChildren = false;
			if (SceneComponent)
			{
				for (USceneComponent* Child : SceneComponent->GetAttachedComponents())
					if (Child && Owned.contains(Child)) bHasChildren = true;
			}

			FOutlinerItem Item;
			Item.Type = EOutlinerItemRowType::Component;
			Item.UUID = Component->GetUUID();
			const char* Name = Component->GetClass() ? Component->GetClass()->GetDisplayName().c_str() : "Component";
			Item.DisplayLabel = (Component == Actor->GetRootComponent() ? FString("[Root] ") : FString())
				+ Name + " (ID: " + std::to_string(Item.UUID) + ")";
			Item.Depth = Depth;
			Item.Component = Component;
			Item.Actor = Actor;
			Item.bHasChildren = bHasChildren;
			DisplayList.push_back(std::move(Item));

			if (SceneComponent && ExpandedComponentUUIDs.contains(Component->GetUUID()))
			{
				for (USceneComponent* Child : SceneComponent->GetAttachedComponents())
					Self(Self, Child, Depth + 1);
			}
		};

		AppendComponent(AppendComponent, Actor->GetRootComponent(), 1);
		for (UActorComponent* Component : Components)
		{
			if (!Component || Component == Actor->GetRootComponent()) continue;
			USceneComponent* SceneComponent = Component->Cast<USceneComponent>();
			// 일반 컴포넌트와 Actor 내부에 부착 부모가 없는 SceneComponent는 최상위 행.
			// 다른 Actor의 컴포넌트는 그 소유 Actor 아래에서 표시한다.
			if (!SceneComponent || !Owned.contains(SceneComponent->GetAttachParent()))
				AppendComponent(AppendComponent, Component, 1);
		}
	}
}
