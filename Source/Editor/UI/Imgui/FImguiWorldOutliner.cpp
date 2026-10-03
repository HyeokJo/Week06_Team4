#include "FImguiWorldOutliner.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/UWorld.h"
#include "ThirdParty/Imgui/imgui.h"
#include <string>
#include <algorithm>

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
	if (SelectedActor)
	{
		if (ImGui::Button("Delete"))
		{
			AActor* ActorToDelete = SelectedActor;
			Editor.UnSelectActor();
			ActorToDelete->Destroy();
			bCacheDirty = true;
		}
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

	auto& Expanded = Item.Type == EOutlinerItemRowType::Actor
		? ExpandedActorUUIDs : ExpandedComponentUUIDs;
	const bool bIsOpen = Expanded.contains(Item.UUID);
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth
		| ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (!Item.bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf;
	if (Item.Type == EOutlinerItemRowType::Actor && Item.Actor == SelectedActor)
		Flags |= ImGuiTreeNodeFlags_Selected;

	// Clipper는 부모 행을 건너뛸 수 있으므로 ImGui의 트리 스택에 의존하지 않는다.
	ImGui::SetNextItemOpen(bIsOpen, ImGuiCond_Always);
	const bool bNowOpen = ImGui::TreeNodeEx(
		reinterpret_cast<void*>(static_cast<uintptr_t>(Item.UUID)), Flags,
		"%s", Item.DisplayLabel.c_str());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		Editor.SelectActor(Item.Actor);

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
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputTextWithHint("##OutlinerFilter", "Search...", FilterBuffer, sizeof(FilterBuffer)))
	{
		CurrentFilterStr = FilterBuffer;

		std::transform(CurrentFilterStr.begin(), CurrentFilterStr.end(), CurrentFilterStr.begin(),
			[](unsigned char c) { return static_cast<char>(::tolower(c)); });

		return true;;
	}

	return false;
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
