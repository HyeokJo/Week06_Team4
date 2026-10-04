#pragma once

#include "Runtime/Geometry/FTransform.h"
#include "ThirdParty/Json/json.hpp"
#include "UObject.h"
#include "UActorComponent.h"

class UWorld;
//class AActor;
class FJsonArchive;

class USceneComponent : public UActorComponent
{
	GENERATED_BODY()
	DECLARE_UCLASS(USceneComponent, UActorComponent)
    friend class AActor;
public:
    virtual void Initialize() override;
    virtual void Release() override;
    
    USceneComponent* GetAttachParent() const { return AttachParent; }

    bool SetupAttachment(USceneComponent* InParent);
    void SetupDetachment(bool bKeepWorldTransform = true);
	virtual void Serialize(FJsonArchive& Archive) const override;
	virtual void Deserialize(const FJsonArchive& Archive) override;
    void SetInheritRotation(bool bInherit);
    //Transform이 바뀔 때 본인 및 자식 컴포넌트에 알림.
    void MarkActorTransformDirty();
protected:
	USceneComponent() = default;

	FTransform RelativeTransform;

    //파생 클래스에서 Transfrom 변경에 따라 반응.
    virtual void OnTransformChanged() {}

public:
	const FTransform& GetRelativeTransform() const { return RelativeTransform; }
	virtual void SetRelativeTransform(const FTransform& RelativeTransform);
	const FTransform& GetGlobalTransform() const;
	const FMatrix& GetGlobalTransformMatrix() const { return GetGlobalTransform().GetMatrix(); }
	// 월드 행렬의 역행렬. 스케일이 0에 가까워 역행렬이 없으면 nullptr.
	const FMatrix* GetGlobalInverseMatrix() const;
	//void SetRelativeTransformFromGlobal(const FTransform& GlobalTransform);

    virtual void SetRelativeLocation(const FVector& RelativeLocation);
    virtual void SetRelativeRotation(const FVector& RelativeRotation);
    virtual void SetRelativeRotation(const FQuaternion& RelativeRotation);
    virtual void SetRelativeScale(const FVector& RelativeScale);

    virtual const FVector& GetRelativeLocation() const;
    virtual const FQuaternion& GetRelativeRotation() const;
    virtual const FVector& GetRelativeScale() const;

    void SetBatchIndex(int32 Index) { BatchIndex = Index; }
    int32 GetBatchIndex() const { return BatchIndex; }

	const TArray<USceneComponent*>& GetAttachedComponents() const { return AttachedComponents; }
protected:
    USceneComponent* AttachParent = nullptr;
    TArray<USceneComponent*> AttachedComponents;
    bool bInheritRotation = true;

    int32 BatchIndex = -1;

private:
    USceneComponent* GetTransformParent() const;

    // 월드 Transform 캐시. 부모의 GlobalVersion이 바뀌면 자식도 자동으로 재계산된다.
    mutable FTransform CachedGlobal;
    mutable FMatrix CachedGlobalInverse;
    mutable uint32 CachedInverseVersion = UINT32_MAX;
    mutable bool bCachedInverseValid = false;   // 역행렬 계산 실패도 캐시한다
    mutable const USceneComponent* CachedParent = nullptr;
    mutable uint32 CachedParentVersion = 0;
    mutable uint32 GlobalVersion = 0;
    mutable bool bGlobalDirty = true;
};
