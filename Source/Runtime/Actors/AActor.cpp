#include "AActor.h"
#include "Runtime/Core/Log.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Engine/UWorld.h"
#include <algorithm>
#include "Runtime/Engine/ULevel.h"

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

void AActor::Serialize(FArchive& Archive) const
{
	Super::Serialize(Archive);

	if (RootComponent)
	{
		FArchive RootArchive{};
		RootComponent->Serialize(RootArchive);
		Archive.SetArchive("RootComponent", RootArchive);
	}
	else
	{
		Archive.SetNull("RootComponent");
	}
}

void AActor::Deserialize(const FArchive& Archive)
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

	FArchive RootComponentArchive = Archive.GetArchive("RootComponent");
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

void AActor::AddComponent(UActorComponent* Addcomp)
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

	if (SceneComp) {
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
	if (!bTickEnabled || !bHasBegunPlay)
	{
		return;
	}

	for (UActorComponent* Component : AttachedComp)
	{
		if (Component && Component->IsTickEnabled())
		{
			Component->Update(DeltaTime);
		}
	}
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
	Owner = nullptr;
}

void AActor::Destroy() {
	if (Owner)
	{
		Owner->GetWorld()->DestroyActor(this);
		return;
	}
	DestroyObject(this);
}
