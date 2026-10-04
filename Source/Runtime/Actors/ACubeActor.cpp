#include "ACubeActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/Mesh/UStaticMeshComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(ACubeActor, AActor)
UCLASS_META(ACubeActor, DisplayName, "Cube Actor")

ACubeActor::ACubeActor()
{
	// 기본 큐브 컴포넌트 장착
	UStaticMeshComponent* Object = NewObjectWithOuter<UStaticMeshComponent>(this);
	SetRootComponent(Object);
}
void ACubeActor::PostInitProperties()
{
	Super::PostInitProperties();

	// 생성자가 만든 Root를 그대로 사용한다.
	UStaticMeshComponent* Component = RootComponent->Cast<UStaticMeshComponent>();
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	Component->SetMesh(Registry.Get<UStaticMesh>("#Cube"));
	Component->SetMaterial(Registry.Get<UMaterial>("Material/Textured.json"));
}