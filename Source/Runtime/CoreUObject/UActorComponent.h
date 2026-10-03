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
public:
	//virtual void Initialize() override;
	virtual void Release() override;

	AActor* GetActorOwner() const { return ActorOwner; };

	virtual void Register(ULevel& InLevel);
	virtual void BeginPlay();
	virtual void Update(float DeltaTime) {}
	virtual void EndPlay();
	virtual void Unregister();

	// TODO : FTick에 Register 하는 형태로 변경해야함.
	//[[nodiscard]] bool IsTickEnabled() const { return bTickEnabled; }
	[[nodiscard]] bool IsRegistered() const { return Level != nullptr; }
	[[nodiscard]] bool HasBegunPlay() const { return bHasBegunPlay; }

	//virtual void Serialize(FArchive& Archive) const override;
	//virtual void Deserialize(const FArchive& Archive) override;
	//일단 임시 조정
	bool IsTickEnabled() const{ return PrimaryComponentTick.bTickEnabled; }
	// Component Tick의 활성 상태를 변경합니다.
	void SetComponentTickEnabled(bool bEnabled);

	// Component Tick의 실행 그룹을 변경합니다.
	void SetComponentTickGroup(ETickGroup Group);

	//TODO : Owner를 액터만 설정할수 있도록,  하단 함수를 private로 걸고, 액터에 등록될때 이 함수가 호출되도록 함.
	//TODO : Outer 구조 도입시 구조 수정해야함.
	void SetActorOwner(AActor* InOwner) { ActorOwner = InOwner; }

protected:
	AActor* ActorOwner = nullptr;
	ULevel* Level = nullptr;
	bool bHasBegunPlay = false;
	//bool bTickEnabled = false;
	FTickSettings PrimaryComponentTick;

private:


};
