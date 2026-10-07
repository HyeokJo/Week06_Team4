#pragma once

#include "UMovementComponent.h"
#include "Runtime/Math/FVector.h"

// UpdatedComponent의 위치를 유지하며 로컬축 또는 월드축 기준으로 자전한다.
class UProjectileMovementComponent : public UMovementComponent
{
    GENERATED_BODY()
    DECLARE_UCLASS(UProjectileMovementComponent, UMovementComponent)

public:
    using Super::Serialize;
    void Serialize(FArchive& Archive) override;
    void BeginPlay() override;
    void EndPlay() override;
    void Update(float DeltaTime) override;

    FVector LaunchDirection = FVector::ForwardVector;   //발사 방향
    float InitialSpeed = 1.0f; // 초기속도
    float MaxSpeed = 0.0f;     // 0 이하이면 속도를 제한하지 않는다.
    bool bInitialVelocityInLocalSpace = true;
    FVector Acceleration = FVector::ZeroVector; //가속도
    //TODO: World가 중력을 갖고난 후에는 객체의 중력배율로 아래 변경
    //TODO: 물리시뮬레이터 제작시 해당 업데이트는 물리시뮬레이터가 담당해야함.
    bool bEnableGravity = false;
    FVector GravityAcceleration = FVector(.0f, .0f, -9.8f); //중력가속도
    void ResetVelocity();
    const FVector& GetVelocity()const { return Velocity; }


protected:
    UProjectileMovementComponent() = default;
private:
    FVector LimitVelocity(const FVector& NewVelocity) const;

    // 실행 상태는 저장·복제하지 않고 새 실행에서 다시 만든다.
    FVector Velocity = FVector::ZeroVector;
    bool bVelocityInitialized = false;

};