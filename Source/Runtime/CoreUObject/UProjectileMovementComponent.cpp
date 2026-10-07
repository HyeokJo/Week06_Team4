#include "UProjectileMovementComponent.h"
#include "USceneComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Engine/FArchive.h"
#include <algorithm>
#include <cmath>

IMPLEMENT_UCLASS(UProjectileMovementComponent, UMovementComponent)

// 기존 Components 폴더와 추가 메뉴가 이 메타데이터를 읽는다.
UCLASS_META(UProjectileMovementComponent, DisplayName, "ProjectileMovement")
UCLASS_META(UProjectileMovementComponent, SpawnableComponent, "true")

void UProjectileMovementComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
    Archive.OptionalField("LaunchDirection", LaunchDirection);
    Archive.OptionalField("InitialSpeed", InitialSpeed);
    Archive.OptionalField("MaxSpeed", MaxSpeed);
    Archive.OptionalField("InitialVelocityInLocalSpace", bInitialVelocityInLocalSpace);
    Archive.OptionalField("Acceleration", Acceleration);
    Archive.OptionalField("bEnableGravity", bEnableGravity);
    Archive.OptionalField("GravityAcceleration", GravityAcceleration);
    if (Archive.IsLoading())
    {
        // 참조 복원이 끝나기 전에는 대상의 Transform으로 속도를 계산하지 않는다.
        Velocity = FVector::ZeroVector;
        bVelocityInitialized = false;
    }
}

void UProjectileMovementComponent::BeginPlay()
{
    if (HasBegunPlay()) return;
    Super::BeginPlay();

    // PIE/Game은 등록과 데이터 복원을 마친 후의 자세로 발사한다.
    if (HasBegunPlay()) ResetVelocity();
}

void UProjectileMovementComponent::EndPlay()
{
    Super::EndPlay();

    // 다시 BeginPlay하더라도 이전 실행의 속도를 이어받지 않는다.
    Velocity = FVector::ZeroVector;
    bVelocityInitialized = false;
}

void UProjectileMovementComponent::ResetVelocity()
{
    Velocity = FVector::ZeroVector;
    bVelocityInitialized = false;

    USceneComponent* Target = GetUpdatedComponent();
    if (!Target) return; // 대상이 나중에 생기면 첫 유효 Update에서 다시 계산한다.

    const float DirectionSizeSquared = LaunchDirection.SizeSquared();
    if (DirectionSizeSquared > 1.0e-8f)
    {
        Velocity = LaunchDirection;
        Velocity.Normalize();
        Velocity *= std::max(0.0f, InitialSpeed);
        // 로컬 방향을 시작할 때 한 번만 변환한다. 대상의 스케일은 적용하지 않는다.
        if (bInitialVelocityInLocalSpace)
            Velocity = Target->GetGlobalTransform().GetRotation().Normalized().RotateVector(Velocity);

        Velocity = LimitVelocity(Velocity);
    }

    // 방향이 0인 경우도 초기화를 완료한 정지 상태로 취급한다.
    bVelocityInitialized = true;
}

FVector UProjectileMovementComponent::LimitVelocity(const FVector& NewVelocity) const
{
    if (MaxSpeed <= 0.0f || NewVelocity.SizeSquared() <= MaxSpeed * MaxSpeed)
        return NewVelocity;

    FVector LimitedVelocity = NewVelocity;
    LimitedVelocity.Normalize();
    return LimitedVelocity * MaxSpeed;
}

void UProjectileMovementComponent::Update(float DeltaTime)
{
    if (ShouldSkipUpdate(DeltaTime)) return;

    // BeginPlay가 없는 Editor 미리보기와, 나중에 대상이 생긴 경우를 처리한다.
    if (!bVelocityInitialized) ResetVelocity();

    Velocity += Acceleration * DeltaTime;
    if (bEnableGravity) Velocity += GravityAcceleration * DeltaTime;
    Velocity = LimitVelocity(Velocity);

    if (Velocity == FVector::ZeroVector) return;

    USceneComponent* Target = GetUpdatedComponent();


    // 회전과 스케일을 유지하며 월드 이동량만 적용한다.
    // 상대값 환산과 Transform/바운드 Dirty 알림은 기존 이동 경로가 처리한다.
    MoveUpdatedComponent(Velocity * DeltaTime, Target->GetGlobalTransform().GetRotation());
}