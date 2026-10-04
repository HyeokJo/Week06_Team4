#include "AAppleBittenActor.h"

#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/Mesh/UStaticMeshComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(AAppleBittenActor, AActor)
UCLASS_META(AAppleBittenActor, DisplayName, "Apple Bitten Actor")

AAppleBittenActor::AAppleBittenActor()
{	
	AppleStaticMeshComp = NewObjectWithOuter<UStaticMeshComponent>(this);
	SetRootComponent(AppleStaticMeshComp);
}

void AAppleBittenActor::Update(float DeltaTime)
{
	Super::Update(DeltaTime);
}
void AAppleBittenActor::OnComponentRemoved(UActorComponent* Component)
{
	// 소유 목록 밖에 보관한 별도 참조도 정리한다.
	Super::OnComponentRemoved(Component);
	if (Component == AppleStaticMeshComp) AppleStaticMeshComp = nullptr;
}

void AAppleBittenActor::PostInitProperties()
{
	Super::PostInitProperties();

	// 기본 컴포넌트의 소유·부착 관계는 생성자가 이미 설정했다.
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	AppleStaticMeshComp->SetMesh(Registry.Get<UStaticMesh>("StaticMesh/JungleApple/Apple_Bitten.json"));
}