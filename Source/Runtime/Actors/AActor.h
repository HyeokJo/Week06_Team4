#pragma once

#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include <type_traits>
#include <concepts>

class UWorld;

class AActor : public UObject
{
	DECLARE_UCLASS(AActor, UObject)
	GENERATED_BODY()

	friend class UWorld;

protected:
	//nullptr일 경우 위치정보가 필요없는 논리적 액터
	USceneComponent* RootComponent = nullptr;
	TArray<UActorComponent*> AttachedComp;
	bool bTickEnabled = false;

	explicit AActor() = default;

	virtual void Serialize(FArchive& Archive) const override;
	virtual void Deserialize(const FArchive& Archive) override;

public:
	void Initialize() override;
	void Release() override;
	UWorld* GetOwner() const { return Owner; }

	void CreateRootComponent(UClass* ClassType);

	void SetRootComponent(USceneComponent* Component);
	USceneComponent* GetRootComponent() const { return RootComponent; }
	const TArray<UActorComponent*>& GetAttachedComponents() const { return AttachedComp; }


	FTransform GetTransform() const { return RootComponent ? RootComponent->GetRelativeTransform() : FTransform{}; }
	void SetTransform(const FTransform& NewTransform) { if (RootComponent) RootComponent->SetRelativeTransform(NewTransform); }

	//하위 컴포넌트 월드 Tranform도 바뀐다.
	void MarkComponentsTransformDirty();

	void AddComponent(UActorComponent* Addcomp);
	virtual void Register(UWorld& World);
	virtual void BeginPlay();
	virtual void Update(float DeltaTime);
	virtual void EndPlay();
	virtual void Unregister();

	[[nodiscard]] bool IsRegistered() const { return Owner != nullptr; }
	[[nodiscard]] bool HasBegunPlay() const { return bHasBegunPlay; }

	void Destroy();

private:
	UWorld* Owner = nullptr; // SpawnActor될 때 설정됨
	bool bHasBegunPlay = false;
};
