#pragma once

#include "UMovementComponent.h"
#include "Runtime/Math/FVector.h"

// UpdatedComponent의 위치를 유지하며 로컬축 또는 월드축 기준으로 자전한다.
class URotationMovementComponent : public UMovementComponent
{
    GENERATED_BODY()
    DECLARE_UCLASS(URotationMovementComponent, UMovementComponent)

public:
    using Super::Serialize;
    void Serialize(FArchive& Archive) override;
    void Update(float DeltaTime) override;

    // 각 축의 회전 속도(도/s). 음수면 반대 방향으로 회전한다.
    FVector RotationRate = FVector::ZeroVector;

    // 로컬축/월드축 기준 회전 여부
    bool bRotationInLocalSpace = true;

protected:
    URotationMovementComponent() = default;
};