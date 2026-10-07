#pragma once

#include "AActor.h"

class UFogComponent;

class AExponentialHeightFogActor : public AActor
{
	DECLARE_UCLASS(AExponentialHeightFogActor, AActor)
	GENERATED_BODY()

public:
	explicit AExponentialHeightFogActor();
	void PostInitProperties() override;
	void Serialize(FArchive& Archive) override;
	//void Initialize() override;
	virtual void Update(float DeltaTime) override;
protected:
	// 기본 컴포넌트가 단독 삭제됐을 때 별도 참조를 비운다.
	void OnComponentRemoved(UActorComponent* Component) override;
private:
	UFogComponent* FogComp;
	bool bIsSpin = false;

	float SpinSpeed = 2000.0f;
	float SpinRate = 2.0f;
	float ElapsedTime = 0.0f;
};
