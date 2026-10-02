#include "USceneComponent.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/FArchive.h"

#include <numbers>


IMPLEMENT_UCLASS(USceneComponent, UActorComponent)

void USceneComponent::Initialize()
{
    Super::Initialize();
    bGlobalDirty = true;
}
void USceneComponent::Release()
{
    // 등록 해제 중에는 기존 소유/부착 관계를 사용할 수 있어야 한다.
    Super::Release();
    AttachParent = nullptr;
    CachedParent = nullptr;
    bGlobalDirty = true;
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
    if (InParent == this) { return; }

    AttachParent = InParent;
    // 부착 관계는 Transform에만 영향을 주며 Actor 소유권은 변경하지 않는다.
    MarkActorTransformDirty();
}

void USceneComponent::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);

    Archive.SetVector("Location", RelativeTransform.GetLocation());
    Archive.SetVector("Rotation", RelativeTransform.GetRotation().GetEulerXYZ());
    Archive.SetVector("Scale", RelativeTransform.GetScale3D());
}

void USceneComponent::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);

    // Location
    RelativeTransform.SetLocation(Archive.GetVector("Location"));

    // Rotation
    constexpr float RadToDeg = 180.0f / std::numbers::pi_v<float>;
    FVector Rotation = Archive.GetVector("Rotation");
    for (int i = 0; i < 3; ++i)
    {
        Rotation[i] *= RadToDeg;
    }
    RelativeTransform.SetRotation(FQuaternion::FromEulerXYZDeg(Rotation));

    // Scale
    RelativeTransform.SetScale3D(Archive.GetVector("Scale"));
    MarkActorTransformDirty();
}

void USceneComponent::SetRelativeTransform(const FTransform& RelativeTransform)
{
    if (this->RelativeTransform == RelativeTransform) { return; }
    this->RelativeTransform = RelativeTransform;
    MarkActorTransformDirty();
}

USceneComponent* USceneComponent::GetTransformParent() const
{
    if (AttachParent)
    {
        return AttachParent;
    }

    AActor* Owner = GetActorOwner();
    if (!Owner)
    {
        return nullptr;
    }

    USceneComponent* Root = Owner->GetRootComponent();
    return Root == this ? nullptr : Root;
}

const FTransform& USceneComponent::GetGlobalTransform() const //나중에 부모 rootcomponent world좌표 써야됨
{
    const USceneComponent* Parent = GetTransformParent();
    uint32 ParentVersion = 0;
    if (Parent)
    {
        Parent->GetGlobalTransform(); // 부모 캐시를 먼저 최신화
        ParentVersion = Parent->GlobalVersion;
    }

    if (!bGlobalDirty && CachedParent == Parent && CachedParentVersion == ParentVersion)
    {
        return CachedGlobal;
    }

    if (!Parent)
    {
        CachedGlobal = RelativeTransform;
    }
    else if (AttachParent || bInheritRotation)
    {
        CachedGlobal = Parent->CachedGlobal * RelativeTransform;
    }
    else
    {
        // 부모 회전 무시 - 위치와 스케일만 상속
        FTransform Result;
        Result.SetScale3D(RelativeTransform.GetScale3D());
        Result.SetRotation(RelativeTransform.GetRotation()); // 자신의 회전만 사용
        Result.SetLocation(Parent->CachedGlobal.GetLocation() + RelativeTransform.GetLocation()); // 월드 축 기준 오프셋
        CachedGlobal = Result;
    }

    CachedGlobal.GetMatrix(); // 행렬도 이 시점에 한 번만 계산해 둔다
    CachedParent = Parent;
    CachedParentVersion = ParentVersion;
    bGlobalDirty = false;
    ++GlobalVersion;
    return CachedGlobal;
}

const FMatrix* USceneComponent::GetGlobalInverseMatrix() const
{
    const FTransform& Global = GetGlobalTransform();
    if (CachedInverseVersion != GlobalVersion)
    {
        // 실패하면 Inverse는 CachedGlobalInverse를 건드리지 않으므로 성공 여부를 따로 기록한다
        bCachedInverseValid = Global.GetMatrix().Inverse(CachedGlobalInverse);
        CachedInverseVersion = GlobalVersion;
    }
    return bCachedInverseValid ? &CachedGlobalInverse : nullptr;
}

void USceneComponent::MarkActorTransformDirty()
{
    bGlobalDirty = true;
    OnTransformChanged();

    if (AActor* Owner = GetActorOwner())
    {
        Owner->MarkComponentsTransformDirty();
    }
}

void USceneComponent::SetRelativeLocation(const FVector& RelativeLocation)
{
    FTransform NewTransform = GetRelativeTransform();
    NewTransform.SetLocation(RelativeLocation);
    SetRelativeTransform(NewTransform);
}

void USceneComponent::SetRelativeRotation(const FVector& RelativeRotationEulerAngle)
{
    FTransform NewTransform = GetRelativeTransform();
    NewTransform.SetRotation(FQuaternion::FromEulerXYZDeg(RelativeRotationEulerAngle));
    SetRelativeTransform(NewTransform);
}

void USceneComponent::SetRelativeRotation(const FQuaternion& RelativeRotation)
{
    FTransform NewTransform = GetRelativeTransform();
    NewTransform.SetRotation(RelativeRotation);
    SetRelativeTransform(NewTransform);
}

void USceneComponent::SetRelativeScale(const FVector& RelativeScale)
{
    FTransform NewTransform = GetRelativeTransform();
    NewTransform.SetScale3D(RelativeScale);
    SetRelativeTransform(NewTransform);
}

const FVector& USceneComponent::GetRelativeLocation() const
{
    return GetRelativeTransform().GetLocation();
}

const FQuaternion& USceneComponent::GetRelativeRotation() const
{
    return GetRelativeTransform().GetRotation();
}

const FVector& USceneComponent::GetRelativeScale() const
{
    return GetRelativeTransform().GetScale3D();
}
