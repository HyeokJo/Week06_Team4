#include "pch.h"
#include "ACatActor.h"
#include "Runtime/Engine/FArchive.h"

#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/Mesh/UStaticMeshComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"

IMPLEMENT_UCLASS(ACatActor, AActor)
UCLASS_META(ACatActor, DisplayName, "Cat Actor")

ACatActor::ACatActor()
{	
	CatStaticMeshComp = NewObjectWithOuter<UStaticMeshComponent>(this);
	SetRootComponent(CatStaticMeshComp);

}
void ACatActor::PostInitProperties()
{
	Super::PostInitProperties();

	// 기본 설정은 복원 전에 적용한다.
	bTickEnabled = true;
	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
	CatStaticMeshComp->SetMesh(Registry.Get<UStaticMesh>("StaticMesh/oiia/oiia.json"));
}
void ACatActor::Update(float DeltaTime)
{
	Super::Update(DeltaTime);
	if (!CatStaticMeshComp) return;
	ElapsedTime += DeltaTime;

	if (ElapsedTime >= SpinRate)
	{
		ElapsedTime = 0.0f;
		
		FAssetRegistry& Registry = FAssetRegistry::GetInstance();
		if (bIsSpin)
		{
			CatStaticMeshComp->SetMesh(Registry.Get<UStaticMesh>("StaticMesh/oiia/oiia.json"));
			bIsSpin = false;
		}
		else
		{
			CatStaticMeshComp->SetMesh(Registry.Get<UStaticMesh>("StaticMesh/oiia/Spin.json"));
			bIsSpin = true;
		}
	}

	if (bIsSpin)
	{
		FTransform CurrentTransform = GetTransform();
		FQuaternion Rotation = CurrentTransform.GetRotation();
		Rotation.RotateLocalAxisAngle(FVector(0.0f, 0.0f, 1.0f), SpinSpeed * DeltaTime);
		CurrentTransform.SetRotation(Rotation);
		SetTransform(CurrentTransform);
	}
}
void ACatActor::OnComponentRemoved(UActorComponent* Component)
{
	// 소유 목록 밖에 보관한 별도 참조도 정리한다.
	Super::OnComponentRemoved(Component);
	if (Component == CatStaticMeshComp) CatStaticMeshComp = nullptr;
}

void ACatActor::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);

	// 파생 Actor가 별도로 보관한 포인터도 새 컴포넌트로 연결한다.
	Archive.Field("CatComponent", CatStaticMeshComp);
	Archive.OptionalField("SpinSpeed", SpinSpeed);
	Archive.OptionalField("SpinRate", SpinRate);
}