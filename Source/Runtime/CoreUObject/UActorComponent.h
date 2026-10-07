#pragma once

#include "UObject.h"
#include "Runtime/Engine/FTick.h"
//#include "Runtime/Actors/AActor.h"

class AActor;
class ULevel;

// AActor에 부착되어 액터의 일부 기능을 수행하는 컴포넌트 클래스입니다.
class UActorComponent : public UObject {
	GENERATED_BODY()
	DECLARE_UCLASS(UActorComponent, UObject)
	friend class AActor;
	friend class UWorld;
public:
	using Super::Serialize;
	virtual void Initialize() override;
	virtual void Release() override;
	void Serialize(FArchive& Archive) override;

	AActor* GetActorOwner() const { return ActorOwner; };

	virtual void Register(ULevel& InLevel);
	virtual void BeginPlay();
	virtual void Update(float DeltaTime) {}
	virtual void EndPlay();
	virtual void Unregister();

	[[nodiscard]] bool IsRegistered() const { return Level != nullptr; }
	[[nodiscard]] bool HasBegunPlay() const { return bHasBegunPlay; }
	[[nodiscard]] bool IsInitialized() const { return bInitialized; }
	bool IsTickEnabled() const { return PrimaryComponentTick.bTickEnabled; }

	// Actor Tick이 꺼져 있어도 Component 자신의 설정으로 실행한다.
	bool bTickInEditor = false;
	bool ShouldTick(bool bEditorWorld) const
	{
		return IsRegistered() && PrimaryComponentTick.bCanEverTick && IsTickEnabled()
			&& (bEditorWorld ? bTickInEditor : HasBegunPlay());
	}

	void SetComponentTickEnabled(bool bEnabled);
	void SetComponentTickGroup(ETickGroup Group);
protected:
	AActor* ActorOwner = nullptr;
	ULevel* Level = nullptr;
	bool bHasBegunPlay = false;
	//bool bTickEnabled = false;
	FTickSettings PrimaryComponentTick;

private:
	//TODO : Outer 구조 도입시 구조 수정해야함.
	void SetActorOwner(AActor* InOwner) { ActorOwner = InOwner; }
	bool bInitialized = false;
	void RefreshTickRegistration();

};
