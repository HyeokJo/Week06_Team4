#pragma once

#include "UActorComponent.h"
#include "TWeakObjectPtr.h"

class USceneComponent;
struct FVector;
struct FQuaternion;

// 위치를 직접 가지지 않고, 같은 Actor의 SceneComponent를 움직이는 공통 부모다.
class UMovementComponent : public UActorComponent
{
    GENERATED_BODY()
    DECLARE_UCLASS(UMovementComponent, UActorComponent)

public:
    using Super::Serialize;
    void PostInitProperties() override;
    void Initialize() override;
    void Register(ULevel& InLevel) override;
    void Serialize(FArchive& Archive) override;

    // 지정된 대상이 없으면 소유 Actor의 Root를 이동 대상으로 사용한다.
    bool bAutoRegisterUpdatedComponent = true;

    // 다른 Actor의 컴포넌트는 거부하고 기존 대상을 유지한다. nullptr는 허용한다.
    bool SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
    USceneComponent* GetUpdatedComponent() const;

    // 월드 이동량과 최종 월드 회전을 적용한다. 대상의 스케일은 유지한다.
    bool MoveUpdatedComponent(const FVector& WorldDelta, const FQuaternion& NewWorldRotation);

protected:
    UMovementComponent() = default;

    // 파생 Update()에서 시간과 이동 대상만 확인한다. Tick 허용 정책은 World가 판단한다.
    bool ShouldSkipUpdate(float DeltaTime);

private:
    void ResolveUpdatedComponent();

    // Actor가 대상을 소유한다. 이동 컴포넌트는 삭제 여부를 확인할 수 있는 참조만 보관한다.
    TWeakObjectPtr<USceneComponent> UpdatedComponent;
};