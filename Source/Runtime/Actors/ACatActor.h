#pragma once

#include "AActor.h"

// 큐브 액터 정의
class UStaticMeshComponent;

class ACatActor : public AActor
{
	DECLARE_UCLASS(ACatActor, AActor)
	GENERATED_BODY()

public:
	explicit ACatActor();
	void PostInitProperties() override;
	//void Initialize() override;
	virtual void Update(float DeltaTime) override;
protected:
	// 기본 컴포넌트가 단독 삭제됐을 때 별도 참조를 비운다.
	void OnComponentRemoved(UActorComponent* Component) override;
private:
	UStaticMeshComponent* CatStaticMeshComp;
	bool bIsSpin = false;

	float SpinSpeed = 2000.0f;
	float SpinRate = 2.0f;
	float ElapsedTime = 0.0f;
};
