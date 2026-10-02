#include "UActorComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"

IMPLEMENT_UCLASS(UActorComponent, UObject)

void UActorComponent::Release()
{
    if (bHasBegunPlay) { EndPlay(); }
    if (Scene) { Unregister(); }

    ActorOwner = nullptr;
    Scene = nullptr;
    Super::Release();
}

void UActorComponent::Register(UScene& InScene)
{
    if (Scene == &InScene) { return; }
    if (Scene) { Unregister(); }
    Scene = &InScene;
}

void UActorComponent::BeginPlay()
{
    if (!Scene || bHasBegunPlay) { return; }
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
    Scene = nullptr;
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
