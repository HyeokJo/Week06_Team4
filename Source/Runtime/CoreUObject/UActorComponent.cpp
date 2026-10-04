#include "UActorComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/ULevel.h"

IMPLEMENT_UCLASS(UActorComponent, UObject)

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
