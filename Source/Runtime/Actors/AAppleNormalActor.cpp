#include "AAppleNormalActor.h"

#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/Mesh/UStaticMeshComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(AAppleNormalActor, AActor)
UCLASS_META(AAppleNormalActor, DisplayName, "Apple Normal Actor")

AAppleNormalActor::AAppleNormalActor()
{	
	AppleStaticMeshComp = NewObjectWithOuter<UStaticMeshComponent>(this);
	SetRootComponent(AppleStaticMeshComp);

	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	AppleStaticMeshComp->SetMesh(Registry.Get<UStaticMesh>("StaticMesh/JungleApple/Apple_Normal.json"));
}

void AAppleNormalActor::Update(float DeltaTime)
{
	Super::Update(DeltaTime);
}
void AAppleNormalActor::OnComponentRemoved(UActorComponent* Component)
{
	// 소유 목록 밖에 보관한 별도 참조도 정리한다.
	Super::OnComponentRemoved(Component);
	if (Component == AppleStaticMeshComp) AppleStaticMeshComp = nullptr;
}
