#include "UActorComponent.h"
#include "Runtime/Engine/FArchive.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/ULevel.h"

IMPLEMENT_UCLASS(UActorComponent, UObject)

void UActorComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);

    // 설정만 저장한다. ActorOwner는 AddComponent(), Level은 Register()에서 연결한다.
    Archive.OptionalField("CanEverTick", PrimaryComponentTick.bCanEverTick);
    Archive.OptionalField("TickEnabled", PrimaryComponentTick.bTickEnabled);
    Archive.OptionalField("TickGroup", PrimaryComponentTick.TickGroup);

    // RegisteredGroup, 초기화·등록·BeginPlay 상태는 저장하지 않는다.
}

void UActorComponent::Initialize()
{
    if (bInitialized) return;

    Super::Initialize();
    bInitialized = true;
}

void UActorComponent::Release()
{
    if (bHasBegunPlay) { EndPlay(); }
    if (Level) { Unregister(); }

	AActor* PreviousOwner = ActorOwner;
    ActorOwner = nullptr;
    Level = nullptr;

    if (PreviousOwner) PreviousOwner->RemoveOwnedComponentReference(this);
    Super::Release();
}

void UActorComponent::Register(ULevel& InLevel)
{
    if (Level == &InLevel) { return; }
    if (Level) { Unregister(); }
    Level = &InLevel;
}

void UActorComponent::BeginPlay()
{
    if (!Level || bHasBegunPlay) { return; }
    bHasBegunPlay = true;
}

void UActorComponent::EndPlay()
{
    if (!bHasBegunPlay) { return; }
    bHasBegunPlay = false;
}

void UActorComponent::Unregister()
{
    if (bHasBegunPlay) { EndPlay(); }
    Level = nullptr;
}

void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
    // Tick Registry 연결 전에는 설정만 보관한다.
    PrimaryComponentTick.bTickEnabled = bEnabled;
}

void UActorComponent::SetComponentTickGroup(ETickGroup Group)
{
    // Count는 실행 그룹이 아닌 미등록 상태를 나타낸다.
    if (static_cast<uint32>(Group) >= TickGroupCount)
    {
        return;
    }
    PrimaryComponentTick.TickGroup = Group;
}
