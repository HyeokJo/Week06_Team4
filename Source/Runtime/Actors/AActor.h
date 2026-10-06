#pragma once

#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include <type_traits>
#include <concepts>

class ULevel;

class AActor : public UObject
{
	DECLARE_UCLASS(AActor, UObject)
	GENERATED_BODY()

	friend class UWorld;
	friend class UActorComponent;

protected:
	//nullptr일 경우 위치정보가 필요없는 논리적 액터
	USceneComponent* RootComponent = nullptr;
	TArray<UActorComponent*> AttachedComp;
	bool bTickEnabled = false;

	explicit AActor() = default;

	virtual void Serialize(FJsonArchive& Archive) const override;
	virtual void Deserialize(const FJsonArchive& Archive) override;

	// 파생 Actor가 별도로 보관하는 컴포넌트 포인터를 정리한다.
	virtual void OnComponentRemoved(UActorComponent* Component) {}
public:
	void Initialize() override;
	void Release() override;
	void Serialize(FArchive& Archive) override;
	ULevel* GetOwner() const { return Owner; }

	void CreateRootComponent(UClass* ClassType);

	void SetRootComponent(USceneComponent* Component);
	USceneComponent* GetRootComponent() const { return RootComponent; }
	const TArray<UActorComponent*>& GetAttachedComponents() const { return AttachedComp; }


	FTransform GetTransform() const { return RootComponent ? RootComponent->GetRelativeTransform() : FTransform{}; }
	void SetTransform(const FTransform& NewTransform) { if (RootComponent) RootComponent->SetRelativeTransform(NewTransform); }

	//하위 컴포넌트 월드 Tranform도 바뀐다.
	void MarkComponentsTransformDirty();
	// 로딩시 복원 전까지 부착을 막기 위함.
	void AddComponent(UActorComponent* Addcomp, bool bAutoAttach = true);	

	void DestroyOwnedComponents();

	virtual void Register(ULevel& InLevel);
	virtual void BeginPlay();
	virtual void Update(float DeltaTime);
	virtual void EndPlay();
	virtual void Unregister();
	 
	[[nodiscard]] bool IsRegistered() const { return Owner != nullptr; }
	[[nodiscard]] bool HasBegunPlay() const { return bHasBegunPlay; }
	[[nodiscard]] bool IsInitialized() const { return bInitialized; }
	[[nodiscard]] bool IsActorTickEnabled() const { return bTickEnabled; }
	[[nodiscard]] bool IsEditorActor() const;

	void Destroy();

private:
	ULevel* Owner = nullptr; // SpawnActor될 때 설정됨
	bool bHasBegunPlay = false;
	//소유권만 삭제, 객체 삭제는 하지않음.
	void RemoveOwnedComponentReference(UActorComponent* Component);

	bool bInitialized = false;
};
