#include "pch.h"
#include "AExponentialHeightFogActor.h"
#include "Runtime/Engine/FArchive.h"

#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UFogComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(AExponentialHeightFogActor, AActor)
UCLASS_META(AExponentialHeightFogActor, DisplayName, "Exponential Height Fog Actor")

AExponentialHeightFogActor::AExponentialHeightFogActor()
{	
	FogComp = NewObjectWithOuter<UFogComponent>(this);
	SetRootComponent(FogComp);

}
void AExponentialHeightFogActor::PostInitProperties()
{
	Super::PostInitProperties();

	// 기본 설정은 복원 전에 적용한다.
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
}
void AExponentialHeightFogActor::Update(float DeltaTime)
{
	Super::Update(DeltaTime);
	if (!FogComp) return;
	ElapsedTime += DeltaTime;
}
void AExponentialHeightFogActor::OnComponentRemoved(UActorComponent* Component)
{
	// 소유 목록 밖에 보관한 별도 참조도 정리한다.
	Super::OnComponentRemoved(Component);
	if (Component == FogComp) FogComp = nullptr;
}

void AExponentialHeightFogActor::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);

	// 파생 Actor가 별도로 보관한 포인터도 새 컴포넌트로 연결한다.
	Archive.Field("FogComponent", FogComp);
	//Archive.OptionalField("SpinSpeed", SpinSpeed);
	//Archive.OptionalField("SpinRate", SpinRate);
}