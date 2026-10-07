#include "AActor.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Core/Log.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/FJsonArchive.h"
#include "Runtime/Engine/UWorld.h"
#include <algorithm>
#include "Runtime/CoreUObject/UMovementComponent.h"


IMPLEMENT_UCLASS(AActor, UObject)

void AActor::Initialize()
{
	if (bInitialized) { return; }
	Super::Initialize();
	for (size_t Index = 0; Index < AttachedComp.size(); ++Index)
	{
		UActorComponent* Component = AttachedComp[Index];
		if (Component)
			Component->Initialize();
	}
	bInitialized = true;
}

void AActor::Release()
{
	ULevel* RegisteredLevel = GetTypedOuter<ULevel>();
	if (bHasBegunPlay)
	{
		EndPlay();
	}

	if (Owner)
	{
		Unregister();
	}

	if (RegisteredLevel)
	{
		RegisteredLevel->GetWorld()->RemoveActor(this);
	}

	while (!AttachedComp.empty())
	{
		UActorComponent* Component = AttachedComp.back();
		std::erase(AttachedComp, Component);

		if (RootComponent == Component)
		{
			RootComponent = nullptr;
		}

		DestroyObject(Component);
	}

	if (RootComponent)
	{
		USceneComponent* RemainingRoot = RootComponent;
		RootComponent = nullptr;
		DestroyObject(RemainingRoot);
	}

	Super::Release();
}

void AActor::Serialize(FJsonArchive& Archive) const
{
	Super::Serialize(Archive);

	if (RootComponent)
	{
		FJsonArchive RootArchive{};
		static_cast<const USceneComponent*>(RootComponent)->Serialize(RootArchive);
		Archive.SetArchive("RootComponent", RootArchive);
	}
	else
	{
		Archive.SetNull("RootComponent");
	}
}

void AActor::Deserialize(const FJsonArchive& Archive)
{
	Super::Deserialize(Archive);

	if (Archive.IsNull("RootComponent"))
	{
		if (RootComponent)
		{
			UE_LOG_WARN("[%s::Deserialize] RootComponent(%s)에 대한 직렬화 데이터가 "
				"누락되었습니다.",
				GetClass()->GetUClassName(),
				RootComponent->GetClass()->GetUClassName());
		}
		return;
	}

	FJsonArchive RootComponentArchive = Archive.GetArchive("RootComponent");
	const FString& SavedTypeName = RootComponentArchive.GetString("Type");
	UClass* SavedClass = UClass::FindByName(SavedTypeName);

	if (SavedClass == nullptr)
	{
		UE_LOG_WARN("[%s::Deserialize] 알 수 없는 타입 %s",
			GetClass()->GetUClassName(), SavedTypeName);
		return;
	}

	if (RootComponent == nullptr)
	{
		CreateRootComponent(SavedClass);

		if (RootComponent == nullptr)
		{
			UE_LOG_WARN("[%s::Deserialize] RootComponent %s를 생성할 수 없습니다.",
				GetClass()->GetUClassName(), SavedTypeName);
			return;
		}
	}

	if (RootComponent->GetClass() != SavedClass)
	{
		UE_LOG_WARN("[%s::Deserialize] 기본 RootComponent (%s)와 저장된 타입 "
			"(%s)가 일치하지 않습니다.",
			GetClass()->GetUClassName(),
			RootComponent->GetClass()->GetUClassName(), SavedTypeName);
		return;
	}

	RootComponent->Deserialize(RootComponentArchive);
}

void AActor::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);

	// 전체 소유 목록과 Root 참조를 저장한다.
	TArray<UActorComponent*> Components = Archive.IsSaving() ? AttachedComp : TArray<UActorComponent*>{};
	USceneComponent* StoredRoot = RootComponent;

	Archive.Field("OwnedComponents", Components);
	Archive.Field("RootComponent", StoredRoot);
	Archive.OptionalField("CanEverTick", PrimaryActorTick.bCanEverTick);
	Archive.OptionalField("TickEnabled", PrimaryActorTick.bTickEnabled);
	Archive.OptionalField("TickGroup", PrimaryActorTick.TickGroup);
	Archive.OptionalField("TickInEditor", bTickInEditor);
	if (Archive.IsLoading())
	{
		// Outer로 생성한 소유 관계와 저장 데이터의 소유 목록을 대조한다.
		if (!std::is_permutation(Components.begin(), Components.end(), AttachedComp.begin(), AttachedComp.end()))
			throw std::runtime_error("Actor component ownership mismatch.");

		if (StoredRoot && std::find(Components.begin(), Components.end(), StoredRoot) == Components.end())
			throw std::runtime_error("RootComponent must be an owned component.");

		AttachedComp = std::move(Components);
		RootComponent = StoredRoot;
		RefreshTickRegistration();
	}

	// Initialize, Register, BeginPlay는 여기서 호출하지 않는다.
}

void AActor::DestroyOwnedComponents()
{
	// 기본 컴포넌트를 제거하면서 파생 Actor가 보관한 별도 참조도 비운다.
	while (!AttachedComp.empty())
	{
		UActorComponent* Component = AttachedComp.back();
		AttachedComp.pop_back();

		if (RootComponent == Component) RootComponent = nullptr;
		OnComponentRemoved(Component);
		DestroyObject(Component);
	}
	RootComponent = nullptr;
}

void AActor::CreateRootComponent(UClass* ClassType)
{
	if (RootComponent) { return; }

	UObject* Object = NewObjectWithOuter(this, ClassType);
	if(!Object)
	{
		return;
	}
	USceneComponent* Component = Object->Cast<USceneComponent>();

	if (!Component)
	{
		DestroyObject(Object);
		return;
	}

	SetRootComponent(Component);
}

void AActor::SetRootComponent(USceneComponent* Component)
{
	if (!Component)
	{
		throw EngineUtil::CreateError("RootComponent가 nullptr입니다.");
	}
	if (RootComponent == Component) { return; }
	if (RootComponent)
	{
		throw EngineUtil::CreateError("이미 Root 컴포넌트가 있습니다.");
	}
	if (Component->GetActorOwner() && Component->GetActorOwner() != this)
	{
		throw EngineUtil::CreateError("다른 Actor가 소유한 컴포넌트입니다.");
	}

	Component->SetupDetachment(false);
	RootComponent = Component;

	AddComponent(Component);
}

void AActor::RemoveOwnedComponentReference(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}
	const bool bWasRoot = RootComponent == Component;
	const auto RemovedCount = std::erase(AttachedComp, Component);

	// 자동승격 구현 안됨
	// TODO: 정책에따라 구현안된상태를 유지할지/아니면 자동승격을 구현할지 결정해야함.
	if (bWasRoot) RootComponent = nullptr;
	if (RemovedCount > 0 || bWasRoot) OnComponentRemoved(Component);

}

bool AActor::DestroyOwnedComponent(UActorComponent* Component)
{
	// 소유 목록의 실제 멤버인 경우에만 단독 삭제 정책을 적용한다.
	if (!Component || Component->GetActorOwner() != this) return false;
	if (std::find(AttachedComp.begin(), AttachedComp.end(), Component) == AttachedComp.end())
		return false;

	if (USceneComponent* SceneComponent = Component->Cast<USceneComponent>())
	{
		USceneComponent* NewParent = SceneComponent->GetAttachParent();
		USceneComponent* PromotedRoot = nullptr;

		// 재부착 중 원본 자식 목록이 바뀌므로 직계 자식 포인터만 복사한다.
		const TArray<USceneComponent*> Children = SceneComponent->GetAttachedComponents();

		if (RootComponent == SceneComponent)
		{
			// UUID 오버레이처럼 다른 객체가 소유한 자식은 Root 후보에서 제외한다.
			for (USceneComponent* Child : Children)
			{
				if (Child && Child->GetAttachParent() == SceneComponent	&& Child->GetActorOwner() == this)
				{
					PromotedRoot = Child;
					break;
				}
			}

			// 직계 자식이 없으면 다른 소유 SceneComponent를 사용한다.
			if (!PromotedRoot)
			{
				for (UActorComponent* OwnedComponent : AttachedComp)
				{
					USceneComponent* Candidate = OwnedComponent	? OwnedComponent->Cast<USceneComponent>() : nullptr;

					if (Candidate && Candidate != SceneComponent)
					{
						PromotedRoot = Candidate;
						break;
					}
				}
			}

			// 이미 소유·초기화·등록된 객체이므로 다시 AddComponent하지 않는다.
			if (PromotedRoot) PromotedRoot->SetupDetachment(true);
			RootComponent = PromotedRoot;
			NewParent = PromotedRoot;
		}

		for (USceneComponent* Child : Children)
		{
			if (!Child || Child == PromotedRoot	|| Child->GetAttachParent() != SceneComponent)
				continue;

			// 직계 자식만 재부착한다. 손자 이하는 기존 계층을 유지한다.
			if (!Child->AttachToComponent(NewParent, true))
			{
				// 새 부모의 역변환이 불가능해도 자식의 월드 Transform은 보존한다.
				Child->SetupDetachment(true);
			}
		}
	}

	// Release가 소유 목록, 파생 Actor 참조, Scene·BVH·Tick 등록을 정리한다.
	DestroyObject(Component);
	return true;
}

void AActor::MarkComponentsTransformDirty()
{
	for (UActorComponent* Component : AttachedComp)
	{
		USceneComponent* SceneComponent = Component ? Component->Cast<USceneComponent>() : nullptr;
		if (!SceneComponent) continue;

		// 같은 Actor가 소유한 조상이 있으면 그 조상의 순회에서 처리된다.
		bool bHasOwnedAncestor = false;
		for (USceneComponent* Parent = SceneComponent->GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
		{
			if (Parent->GetActorOwner() == this)
			{
				bHasOwnedAncestor = true;
				break;
			}
		}

		if (!bHasOwnedAncestor)
			SceneComponent->MarkActorTransformDirty();
	}
}

void AActor::AddComponent(UActorComponent* Addcomp, bool bAutoAttach)
{
	if (Addcomp == nullptr)
	{
		return;
	}
	// 이미 ActorOwner가 있는 컴포넌트 예외처리
	if (Addcomp->ActorOwner && Addcomp->ActorOwner != this)
		throw EngineUtil::CreateError("다른 Actor가 소유한 컴포넌트입니다.");

	// 중복 추가와 중복 Initialize를 방지한다.
	if (std::find(AttachedComp.begin(), AttachedComp.end(), Addcomp) != AttachedComp.end())
		return;

	USceneComponent* SceneComp = Addcomp->Cast<USceneComponent>();

	if (SceneComp && bAutoAttach) {
		if (!RootComponent) {
			SceneComp->SetupDetachment(false);
			RootComponent = SceneComp;

		}
		else if (SceneComp != RootComponent && !SceneComp->GetAttachParent())
		{
			// 부모를 지정하지 않은 추가 SceneComponent는 Root에 부착한다.
			if (!SceneComp->SetupAttachment(RootComponent))
				throw EngineUtil::CreateError("컴포넌트 부착 관계에 순환이 발생합니다.");
		}

	}
	Addcomp->ActorOwner = this;
	AttachedComp.push_back(Addcomp);
	if (!bInitialized) { return; }
	Addcomp->Initialize();

	if (Owner)
	{
		Addcomp->Register(*Owner);
	}

	if (bHasBegunPlay)
	{
		Addcomp->BeginPlay();
	}
}

void AActor::Register(ULevel& InLevel)
{
	if (!bInitialized||Owner == &InLevel)
	{
		return;
	}

	if (Owner)
	{
		Unregister();
	}

	Owner = &InLevel;
	RefreshTickRegistration();
	for (UActorComponent* Component : AttachedComp)
	{
		if (Component)
		{
			Component->Register(InLevel);
		}
	}
}

void AActor::BeginPlay() {
	if (!Owner || bHasBegunPlay)
	{
		return;
	}

	bHasBegunPlay = true;
	for (UActorComponent* Component : AttachedComp)
	{
		if (Component)
		{
			Component->BeginPlay();
		}
	}
}

void AActor::Update(float DeltaTime) {
	// Component들의 Tick은 World의 Tick Registry에서 처리함.
	// 파생 Actor 자신의 로직만 추가할것.
}

void AActor::EndPlay() {
	if (!bHasBegunPlay)
	{
		return;
	}

	for (auto It = AttachedComp.rbegin(); It != AttachedComp.rend(); ++It)
	{
		if (*It)
		{
			(*It)->EndPlay();
		}
	}
	bHasBegunPlay = false;
}

void AActor::Unregister() {
	if (bHasBegunPlay)
	{
		EndPlay();
	}

	if (!Owner)
	{
		return;
	}

	for (auto It = AttachedComp.rbegin(); It != AttachedComp.rend(); ++It)
	{
		if (*It)
		{
			(*It)->Unregister();
		}
	}
	Owner->GetWorld()->RefreshActorTick(this, false);
	Owner = nullptr;
}

// 등록된 Actor만 해당 World의 레지스트리를 갱신한다.
void AActor::RefreshTickRegistration()
{
	if (Owner) Owner->GetWorld()->RefreshActorTick(this, true);
}

void AActor::SetActorTickEnabled(bool bEnabled)
{
	// 활성화 여부만 바꾼다. Component Tick 설정은 변경하지 않는다.
	PrimaryActorTick.bTickEnabled = bEnabled;
	RefreshTickRegistration();
}

void AActor::SetActorTickGroup(ETickGroup Group)
{
	// Count는 실행 그룹으로 지정할 수 없다.
	if (static_cast<uint32>(Group) >= TickGroupCount) return;
	PrimaryActorTick.TickGroup = Group;
	RefreshTickRegistration();
}

void AActor::Destroy() {
	if (Owner)
	{
		Owner->GetWorld()->DestroyActor(this);
		return;
	}
	DestroyObject(this);
}

[[nodiscard]] bool AActor::IsEditorActor() const
{
	ULevel* Level = GetOwner();

	if (!Level) return false;

	UWorld* World = Level->GetWorld();

	if (!World) return false;

	return World->GetWorldType() == EWorldType::Editor; 
}

void AActor::OnComponentRemoved(UActorComponent* RemovedComponent)
{
	// 삭제된 대상을 사용하는 Movement만 정리한다.
	for (UActorComponent* Component : AttachedComp)
	{
		UMovementComponent* Movement = Component ? Component->Cast<UMovementComponent>() : nullptr;

		if (!Movement || Movement->GetUpdatedComponent() != RemovedComponent)
			continue;

		// 다른 Root를 갑자기 움직이지 않도록 자동 대상 선택도 끈다.
		Movement->SetUpdatedComponent(nullptr);
		Movement->bAutoRegisterUpdatedComponent = false;
	}
}