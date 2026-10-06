#include "URotationMovementComponent.h"
#include "USceneComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Engine/FArchive.h"

IMPLEMENT_UCLASS(URotationMovementComponent, UMovementComponent)

// 기존 Components 폴더와 추가 메뉴가 이 메타를 읽어 목록에 표시한다.
UCLASS_META(URotationMovementComponent, DisplayName, "RotatingMovement")
UCLASS_META(URotationMovementComponent, SpawnableComponent, "true")

void URotationMovementComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
    Archive.OptionalField("RotationRate", RotationRate);
    Archive.OptionalField("RotationInLocalSpace", bRotationInLocalSpace);
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

    // 자전이므로 위치를 유지한다. 상대 회전 환산과 Dirty 처리는 부모 함수가 수행한다.
    MoveUpdatedComponent(FVector::ZeroVector, NewWorldRotation);
}