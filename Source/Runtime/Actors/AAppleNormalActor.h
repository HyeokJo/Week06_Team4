#pragma once

#include "AActor.h"

class UStaticMeshComponent;

class AAppleNormalActor : public AActor
{
	DECLARE_UCLASS(AAppleNormalActor, AActor)
	GENERATED_BODY()

public:
	explicit AAppleNormalActor();
	void PostInitProperties() override;
	void Serialize(FArchive& Archive) override;
	virtual void Update(float DeltaTime) override;
protected:
	// 기본 컴포넌트가 단독 삭제됐을 때 별도 참조를 비운다.
	void OnComponentRemoved(UActorComponent* Component) override;
private:
	UStaticMeshComponent* AppleStaticMeshComp;
};
