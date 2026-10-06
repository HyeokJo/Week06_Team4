#include "UMovementComponent.h"
#include "USceneComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/FArchive.h"
#include <stdexcept>

IMPLEMENT_UCLASS(UMovementComponent, UActorComponent)

void UMovementComponent::PostInitProperties()
{
    Super::PostInitProperties();

    // 팩토리 생성 시 기본 Tick 설정만 준비한다. 아직 Owner/Outer는 연결되지 않았다.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEnabled = true;
}

void UMovementComponent::Initialize()
{
    if (IsInitialized()) return;
    Super::Initialize();

    // AddComponent()가 소유자를 연결하고, 로더가 참조 복원을 마친 뒤 호출된다.
    ResolveUpdatedComponent();
}

void UMovementComponent::Register(ULevel& InLevel)
{
    Super::Register(InLevel);

    // 재등록하거나 Initialize 이후 Root가 생긴 경우에도 기본 대상을 연결한다.
    ResolveUpdatedComponent();
}

void UMovementComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
    Archive.OptionalField("AutoRegisterUpdatedComponent", bAutoRegisterUpdatedComponent);

    // Weak 포인터 내부 UUID를 저장하지 않고 기존 Archive의 객체 참조 처리를 사용한다.
    USceneComponent* Target = GetUpdatedComponent();
    Archive.OptionalField("UpdatedComponent", Target);
    if (Archive.IsLoading() && !SetUpdatedComponent(Target))
        throw std::runtime_error("Movement target must belong to its owning Actor.");

    // PIE Reader에서는 Target이 원본 대신 복제된 SceneComponent를 가리킨다.
    // Root 자동 선택은 다른 객체의 복원이 끝나는 Initialize/Register 시점에 처리한다.
}

bool UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    // 대상 지정은 AddComponent()로 소유자를 연결한 다음 수행한다.
    AActor* Owner = GetActorOwner();
    if (NewUpdatedComponent && (!Owner || NewUpdatedComponent->GetActorOwner() != Owner))
        return false;

    UpdatedComponent = NewUpdatedComponent;
    return true;
}

USceneComponent* UMovementComponent::GetUpdatedComponent() const
{
    // 대상이 삭제되었으면 기존 Weak 포인터가 nullptr를 반환한다.
    return UpdatedComponent.Get();
}

void UMovementComponent::ResolveUpdatedComponent()
{
    // 명시적으로 지정한 대상은 유지하고, 비어 있을 때만 Root를 선택한다.
    if (!bAutoRegisterUpdatedComponent || GetUpdatedComponent()) return;
    if (AActor* Owner = GetActorOwner())
        SetUpdatedComponent(Owner->GetRootComponent());
}

bool UMovementComponent::ShouldSkipUpdate(float DeltaTime)
{
    if (DeltaTime <= 0.0f) return true;

    // Root 없이 생성된 Actor에 나중에 SceneComponent를 추가한 경우를 처리한다.
    ResolveUpdatedComponent();
    return GetUpdatedComponent() == nullptr;
}

// WorldDelta = ,NewWorldRotation
bool UMovementComponent::MoveUpdatedComponent(const FVector& WorldDelta, const FQuaternion& NewWorldRotation)
{
    USceneComponent* Target = GetUpdatedComponent();
    if (!Target) return false;

    // 현재 월드 스케일을 보존하고 위치와 회전만 변경한다.
    FTransform WorldTransform = Target->GetGlobalTransform();
    WorldTransform.SetLocation(WorldTransform.GetLocation() + WorldDelta);
    WorldTransform.SetRotation(NewWorldRotation.Normalized());

    // 상대값 환산과 자손 Transform/바운드 Dirty 알림은 기존 SceneComponent가 처리한다.
    return Target->SetWorldTransform(WorldTransform);
}