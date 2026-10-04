#include "ASphereActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/Mesh/UStaticMeshComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(ASphereActor, AActor)
UCLASS_META(ASphereActor, DisplayName, "Sphere Actor")

ASphereActor::ASphereActor()
{
	// 기본 구체 컴포넌트 장착
	UStaticMeshComponent* Object = NewObjectWithOuter<UStaticMeshComponent>(this);
	SetRootComponent(Object);
}
void ASphereActor::PostInitProperties()
{
	Super::PostInitProperties();

	// 기본 구체 메시와 Material을 지정한다.
	UStaticMeshComponent* Component = RootComponent->Cast<UStaticMeshComponent>();
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	Component->SetMesh(Registry.Get<UStaticMesh>("#Sphere"));
	Component->SetMaterial(Registry.Get<UMaterial>("Material/Textured.json"));
}
