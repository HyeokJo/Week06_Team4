#include "UActorComponent.h"
#include "Runtime/Engine/FArchive.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/ULevel.h"
#include "Runtime/Engine/UWorld.h"
IMPLEMENT_UCLASS(UActorComponent, UObject)

void UActorComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);

    // 설정만 저장한다. ActorOwner는 AddComponent(), Level은 Register()에서 연결한다.
    Archive.OptionalField("CanEverTick", PrimaryComponentTick.bCanEverTick);
    Archive.OptionalField("TickEnabled", PrimaryComponentTick.bTickEnabled);
    Archive.OptionalField("TickGroup", PrimaryComponentTick.TickGroup);
    Archive.OptionalField("TickInEditor", bTickInEditor);
    if (Archive.IsLoading()) RefreshTickRegistration();
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

bool UActorComponent::DestroyComponent()
{
    // 소유·Root 정책은 소유 Actor가 판단한다.
    if (AActor* Owner = GetActorOwner())
        return Owner->DestroyOwnedComponent(this);

    // 소유 Actor가 없는 컴포넌트도 기존 Release 경로로 폐기한다.
    DestroyObject(this);
    return true; // 폐기 후에는 this의 멤버를 접근하지 않는다.
}

void UActorComponent::Register(ULevel& InLevel)
{
    if (Level == &InLevel) { return; }
    if (Level) { Unregister(); }
    Level = &InLevel;
    RefreshTickRegistration();
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
    if (Level) Level->GetWorld()->RefreshComponentTick(this, false);
    Level = nullptr;
}

void UActorComponent::RefreshTickRegistration()
{
    // 소속 World가 생긴 뒤에만 목록을 갱신한다.
    if (Level) Level->GetWorld()->RefreshComponentTick(this, true);
}

void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
    PrimaryComponentTick.bTickEnabled = bEnabled;
    RefreshTickRegistration();
}

void UActorComponent::SetComponentTickGroup(ETickGroup Group)
{
    // Count는 실행 그룹이 아닌 미등록 상태를 나타낸다.
    if (static_cast<uint32>(Group) >= TickGroupCount) return;
    PrimaryComponentTick.TickGroup = Group;
    RefreshTickRegistration();
}
