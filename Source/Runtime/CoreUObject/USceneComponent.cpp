#include "USceneComponent.h"
#include "Runtime/Engine/FArchive.h"
#include "UObjectGlobals.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/FJsonArchive.h"

#include <numbers>
#include <algorithm>
#include <cmath>

IMPLEMENT_UCLASS(USceneComponent, UActorComponent)

// 메시 없이 부착점으로 사용할 수 있는 컴포넌트다.
UCLASS_META(USceneComponent, DisplayName, "Scene")
UCLASS_META(USceneComponent, SpawnableComponent, "true")

void USceneComponent::Initialize()
{
    if (IsInitialized()) return;

    Super::Initialize();
    bGlobalDirty = true;
}
void USceneComponent::Release()
{
    if (bHasBegunPlay) EndPlay();
    if (Level) Unregister();

    while (!AttachedComponents.empty())
    {
        USceneComponent* Child = AttachedComponents.back();
        if (Child && Child->AttachParent == this)
            Child->SetupDetachment(true);
        else
            AttachedComponents.pop_back();
    }

    // 자식의 World Transform을 보존한 다음 자신의 부모 연결을 끊는다.
    if (AttachParent) std::erase(AttachParent->AttachedComponents, this);
    AttachParent = nullptr;
    CachedParent = nullptr;
    bGlobalDirty = true;

    Super::Release();
}

bool USceneComponent::SetupAttachment(USceneComponent* InParent)
{
    if (InParent == AttachParent) { return true; }

    // 자기 자신이나 자손에 부착하면 Transform 계산과 트리 순회가 순환한다.
    for (USceneComponent* Parent = InParent; Parent; Parent = Parent->GetTransformParent())
    {
        if (Parent == this) { return false; }
    }

    if (InParent) InParent->AttachedComponents.push_back(this);
    if (AttachParent)
    {
        std::erase(AttachParent->AttachedComponents, this);
    }
    AttachParent = InParent;
    // 부착 관계는 Transform에만 영향을 주며 Actor 소유권은 변경하지 않는다.
    MarkActorTransformDirty();
    return true;
}

void USceneComponent::SetupDetachment(bool bKeepWorldTransform)
{
    if (!AttachParent) return;

    // 부모를 끊기 전에 World Transform을 확보한다.
    const FTransform NewRelative = bKeepWorldTransform ? GetGlobalTransform() : RelativeTransform;

    std::erase(AttachParent->AttachedComponents, this);
    AttachParent = nullptr;
    RelativeTransform = NewRelative;
    MarkActorTransformDirty();
}

void USceneComponent::Serialize(FJsonArchive& Archive) const
{
    Super::Serialize(Archive);

    Archive.SetVector("Location", RelativeTransform.GetLocation());
    Archive.SetVector("Rotation", RelativeTransform.GetRotation().GetEulerXYZ());
    Archive.SetVector("Scale", RelativeTransform.GetScale3D());
}

void USceneComponent::Deserialize(const FJsonArchive& Archive)
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

void USceneComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);

    // 부착 관계는 Outer와 별도로 저장한다. 자식 목록은 부모 연결에서 재구성한다.
    USceneComponent* Parent = AttachParent;
    Archive.Field("AttachParent", Parent);
    Archive.OptionalField("InheritRotation", bInheritRotation);

    FVector Location = RelativeTransform.GetLocation();
    FVector Scale = RelativeTransform.GetScale3D();
    // 내부 회전은 Quaternion으로 유지한다.
    FQuaternion Rotation = RelativeTransform.GetRotation();
    Archive.OptionalField("Location", Location);

    if (Archive.IsDuplicating())
    {
        // PIE 복제에서는 Euler 변환 없이 원본의 네 값을 전달한다.
        FVector4 StoredRotation{ Rotation.X, Rotation.Y, Rotation.Z, Rotation.W };
        if (Archive.OptionalField("Rotation", StoredRotation) && Archive.IsLoading())
        {
            Rotation = FQuaternion(
                StoredRotation.X, StoredRotation.Y, StoredRotation.Z, StoredRotation.W);
        }
    }
    else
    {
        // 씬 파일은 XYZ Euler 각도를 라디안으로 저장한다.
        FVector EulerRotation = Rotation.GetEulerXYZ();
        if (Archive.OptionalField("Rotation", EulerRotation) && Archive.IsLoading())
        {
            // FromEulerXYZDeg()는 도 단위를 받으므로 읽은 라디안을 변환한다.
            constexpr float RadToDeg = 180.0f / std::numbers::pi_v<float>;
            Rotation = FQuaternion::FromEulerXYZDeg(EulerRotation * RadToDeg);
        }
    }

    Archive.OptionalField("Scale", Scale);

    if (Archive.IsLoading())
    {
        // 기존 함수가 순환 검사와 양쪽 부착 목록 갱신을 담당한다.
        if (!SetupAttachment(Parent))
            throw std::runtime_error("Scene component attachment cycle.");

        FTransform Transform;
        Transform.SetLocation(Location);
        // Archive에서 복원한 Quaternion을 적용한다.
        Transform.SetRotation(Rotation);
        Transform.SetScale3D(Scale);
        SetRelativeTransform(Transform);
    }

    // AttachedComponents, 월드 Transform 캐시, BatchIndex는 저장하지 않는다.
}

void USceneComponent::SetInheritRotation(bool bInherit)
{
    if (bInheritRotation == bInherit) return;

    // 부모를 해석하는 방법이 달라지므로 자손까지 갱신한다.
    bInheritRotation = bInherit;
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
    // 부착 부모가 없으면 독립된 Transform이다.
    return AttachParent;
}
const FTransform& USceneComponent::GetGlobalTransform() const
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
    else if (bInheritRotation)
    {
        CachedGlobal = Parent->CachedGlobal * RelativeTransform;
    }
    else
    {
        // 부모 위치만 따라가며 회전과 스케일은 자신의 값을 사용.
        CachedGlobal = RelativeTransform;
        CachedGlobal.SetLocation(Parent->CachedGlobal.GetLocation() + RelativeTransform.GetLocation());
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

bool USceneComponent::SetWorldTransform(const FTransform& GlobalTransform)
{
    FTransform NewRelative = GlobalTransform;
	const USceneComponent* Parent = GetTransformParent();
	if (Parent)
	{
		const FTransform& ParentGlobal = Parent->GetGlobalTransform();
        if (bInheritRotation)
        {
			const FVector& ParentScale = ParentGlobal.GetScale3D();
			constexpr float Epsilon = 1e-8f;
			if (std::abs(ParentScale.X) < Epsilon || 
                std::abs(ParentScale.Y) < Epsilon || 
                std::abs(ParentScale.Z) < Epsilon)
			{
                return false;
			}
			FQuaternion InverseParentRotation = ParentGlobal.GetRotation().Normalized().Conjugate();

            const FVector Offset = GlobalTransform.GetLocation()-ParentGlobal.GetLocation();
			NewRelative.SetLocation(InverseParentRotation.RotateVector(Offset) / ParentScale);

            NewRelative.SetRotation((InverseParentRotation * GlobalTransform.GetRotation()).Normalized());
            NewRelative.SetScale3D(GlobalTransform.GetScale3D() / ParentScale);
        }
        else
        {
			NewRelative.SetLocation(GlobalTransform.GetLocation() - ParentGlobal.GetLocation());
        }
	}
	SetRelativeTransform(NewRelative);
	return true;
}

void USceneComponent::MarkActorTransformDirty()
{
    bGlobalDirty = true;
    OnTransformChanged();

    for (USceneComponent* Child : AttachedComponents)
    {
        if (Child && Child->AttachParent == this)
            Child->MarkActorTransformDirty();
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

bool USceneComponent::AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform)
{
    // 부착 또는 상대 Transform 환산에 실패하면 기존 상태로 돌아간다.
    USceneComponent* PreviousParent = AttachParent;
    const FTransform PreviousRelative = RelativeTransform;
    const FTransform PreviousWorld = GetGlobalTransform();

    if (!SetupAttachment(InParent)) return false;
    if (!bKeepWorldTransform || SetWorldTransform(PreviousWorld)) return true;

    SetupAttachment(PreviousParent);
    SetRelativeTransform(PreviousRelative);
    return false;
}