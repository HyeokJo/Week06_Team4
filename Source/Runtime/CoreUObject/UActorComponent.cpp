#include "UActorComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/UWorld.h"

IMPLEMENT_UCLASS(UActorComponent, UObject)

void UActorComponent::Release()
{
    if (bHasBegunPlay) { EndPlay(); }
    if (World) { Unregister(); }

    ActorOwner = nullptr;
    World = nullptr;
    Super::Release();
}

void UActorComponent::Register(UWorld& InWorld)
{
    if (World == &InWorld) { return; }
    if (World) { Unregister(); }
    World = &InWorld;
}

void UActorComponent::BeginPlay()
{
    if (!World || bHasBegunPlay) { return; }
    bHasBegunPlay = true;
}

void UActorComponent::EndPlay()
{
    if (!bHasBegunPlay) { return; }
    bHasBegunPlay = false;
}

void UActorComponent::Unregister()
{
    if (bHasBegunPlay){ EndPlay(); }
    World = nullptr;
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
