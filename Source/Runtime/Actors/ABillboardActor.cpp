#include "ABillboardActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UBillboardComp.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(ABillboardActor, AActor)
UCLASS_META(ABillboardActor, DisplayName, "Billboard Actor")

ABillboardActor::ABillboardActor()
{
	// 기본 큐브 컴포넌트 장착
	UBillBoardComp* Object = NewObjectWithOuter<UBillBoardComp>(this);
	SetRootComponent(Object);
}
void ABillboardActor::PostInitProperties()
{
	Super::PostInitProperties();

	// 컴포넌트의 기본 Material 준비가 끝난 상태에서 텍스처를 지정한다.
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	GetBillboardComponent()->SetTexture(Registry.Get<UTexture>("Texture/Space.json"));
}
UBillBoardComp* ABillboardActor::GetBillboardComponent() const
{
	return RootComponent ? RootComponent->Cast<UBillBoardComp>() : nullptr;
}
