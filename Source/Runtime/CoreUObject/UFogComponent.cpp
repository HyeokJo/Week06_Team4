#include "UFogComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/ShaderConstants.h"
//#include "Runtime/Engine/UScene.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Asset/UStaticMesh.h"

IMPLEMENT_UCLASS(UFogComponent, USceneComponent)

//void UFogComponent::Initialize()
//{
//	Super::Initialize();
//	//RenderData.Type = ERenderType::Primitive;
//
//	FAssetRegistry& Registry = FAssetRegistry::GetInstance();
//	FMaterialInstance DefaultMaterial{ Registry.Get<UMaterial>("Material/Simple.json") };
//
//}
//
//void UFogComponent::SetColor(const FVector4& Color, int32 Index)
//{
//
//}
//
//void UFogComponent::Register(UScene& InScene)
//{
//	Super::Register(InScene);
//	//InScene.AddRenderComponent(this);
//}
//
//void UFogComponent::Unregister()
//{
//	if (Scene)
//	{
//		//Scene->RemoveRenderComponent(this);
//	}
//	Super::Unregister();
//}

