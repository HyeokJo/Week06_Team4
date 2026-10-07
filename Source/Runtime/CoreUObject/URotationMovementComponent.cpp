#include "URotationMovementComponent.h"
#include "USceneComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Engine/FArchive.h"

IMPLEMENT_UCLASS(URotationMovementComponent, UMovementComponent)

// 기존 Components 폴더와 추가 메뉴가 이 메타를 읽어 목록에 표시한다.
UCLASS_META(URotationMovementComponent, DisplayName, "RotationMovement")
UCLASS_META(URotationMovementComponent, SpawnableComponent, "true")

void URotationMovementComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
    Archive.OptionalField("RotationRate", RotationRate);
    Archive.OptionalField("RotationInLocalSpace", bRotationInLocalSpace);
    Archive.OptionalField("PivotTranslation", PivotTranslation);
}

void URotationMovementComponent::Update(float DeltaTime)
{
    if (ShouldSkipUpdate(DeltaTime)) return;
    if (RotationRate == FVector::ZeroVector) return;

    USceneComponent* Target = GetUpdatedComponent();
    const FQuaternion CurrentRotation = Target->GetGlobalTransform().GetRotation().Normalized();

    // 기존 상대 회전 UI와 같은 XYZ 각도 규칙으로 이번 프레임의 증분을 만든다.
    const FQuaternion DeltaRotation =
        FQuaternion::FromEulerXYZDeg(RotationRate * DeltaTime);

    // Quaternion 곱은 교환되지 않는다. 곱하는 순서로 회전축 기준을 결정한다.
    const FQuaternion NewWorldRotation = bRotationInLocalSpace
        ? CurrentRotation * DeltaRotation   // 로컬축 기준 회전
        : DeltaRotation * CurrentRotation;  // 월드축 기준 회전

    //피벗이 0일때는 자전 유지
    FVector WorldDelta = FVector::ZeroVector;
    if (PivotTranslation != FVector::ZeroVector)
    {
        // 회전 전후의 피벗 오프셋 차이만큼 대상 원점을 이동시킨다.
        // 대상과 부모의 스케일은 피벗 오프셋에 곱하지 않는다.
        WorldDelta = CurrentRotation.RotateVector(PivotTranslation) - NewWorldRotation.RotateVector(PivotTranslation);
    }

    MoveUpdatedComponent(WorldDelta, NewWorldRotation);
}