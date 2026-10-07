#include "AAnimatedBillboardActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UAnimatedBillboardComp.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(AAnimatedBillboardActor, AActor)
UCLASS_META(AAnimatedBillboardActor, DisplayName, "Animated Billboard Actor")

AAnimatedBillboardActor::AAnimatedBillboardActor()
{
	// 루트 컴포넌트 생성 및 장착
	CreateRootComponent(UAnimatedBillboardComp::StaticClass());
}
void AAnimatedBillboardActor::PostInitProperties()
{
	Super::PostInitProperties();

	UAnimatedBillboardComp* Component = GetAnimatedBillboardComponent();
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();

	Component->SetTexture(Registry.Get<UTexture>("Texture/Explosion.json"));
	Component->SetSpriteSheet(6, 6, 20.0f, 36);
	Component->SetLooping(true);
	Component->Play();
}

UAnimatedBillboardComp* AAnimatedBillboardActor::GetAnimatedBillboardComponent() const
{
	return RootComponent ? RootComponent->Cast<UAnimatedBillboardComp>() : nullptr;
}
