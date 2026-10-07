#include "UBillBoardComp.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Engine/UWorld.h"
#include "Runtime/Engine/FJsonArchive.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Engine/FSceneView.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include "Runtime/Asset/UTexture.h"
#include "UClass.h"
#include <algorithm>
#include <cctype>

IMPLEMENT_UCLASS(UBillBoardComp, UPrimitiveComponent)
UCLASS_META(UBillBoardComp, DisplayName, "BillBoard")
UCLASS_META(UBillBoardComp, MeshName, "BillBoard")

// 기존 DisplayName과 기본 에셋 설정은 유지한다.
UCLASS_META(UBillBoardComp, SpawnableComponent, "true")

void UBillBoardComp::PostInitProperties()
{
    Super::PostInitProperties();

    // 부모가 준비한 Material 슬롯에 Billboard 기본 에셋을 지정한다.
    FAssetRegistry& Registry = FAssetRegistry::GetInstance();
    SetMesh(Registry.Get<UStaticMesh>("#Rect"));
    SetMaterial(Registry.Get<UMaterial>("Material/Billboard.json"));
}

//void UBillBoardComp::Initialize() {
//  Super::Initialize();
//
//  FAssetRegistry& Registry = FAssetRegistry::GetInstance();
//  SetMesh(Registry.Get<UStaticMesh>("#Rect"));
//  SetMaterial(Registry.Get<UMaterial>("Material/Billboard.json"));
//
//  RenderData.Type = ERenderType::Primitive;
//}

void UBillBoardComp::Serialize(FJsonArchive& Archive) const
{
    Super::Serialize(Archive);

    UTexture* Texture = GetTexture();
    if (Texture)
    {
        Archive.SetString("TextureAsset", Texture->GetID().ToString());
    }
}

void UBillBoardComp::Deserialize(const FJsonArchive& Archive)
{
    Super::Deserialize(Archive);

    if (Archive.IsNull("TextureAsset"))
    {
        return;
    }

    FAssetRegistry& Registry = FAssetRegistry::GetInstance();
    FString TextureAssetID = Archive.GetString("TextureAsset");
    UTexture* Texture = Registry.Get<UTexture>(TextureAssetID);

    if (Texture)
    {
        SetTexture(Texture);
    }
}

void UBillBoardComp::SetTexture(UTexture* Texture)
{
    UPrimitiveComponent::SetTexture(Texture);
}

UTexture* UBillBoardComp::GetTexture() const
{
    return RenderData.Materials.empty() ? nullptr : RenderData.Materials[0].Texture;
}

FMatrix UBillBoardComp::GetRenderMatrix(const FCamera& Camera) const
{
    FTransform Transform = GetGlobalTransform();

    FMatrix CameraRotation = Camera.GetRotationMatrix();
    FVector ViewForward = CameraRotation.TransformPointRow(FVector{ 1.0f, 0.0f, 0.0f }, 0.0f); // X+
    FVector ViewRight = CameraRotation.TransformPointRow(FVector{ 0.0f, 1.0f, 0.0f }, 0.0f); // Y+
    FVector ViewUp = CameraRotation.TransformPointRow(FVector{ 0.0f, 0.0f, 1.0f }, 0.0f); // Z+

    FVector Up = ViewUp * Transform.GetScale3D().Z;
    FVector Right = ViewRight * Transform.GetScale3D().Y;

    return FMatrix
    {
        FVector4{ ViewForward, 0.0f },
        FVector4{ Right, 0.0f },
        FVector4{ Up, 0.0f },
        FVector4{ Transform.GetLocation(), 1.0f },
    };
}

void UBillBoardComp::SetUVScale(FVector2 Value)
{
    RenderData.Materials[0].UVScale = Value;
}

void UBillBoardComp::SetUVOffset(FVector2 Value)
{
    RenderData.Materials[0].UVOffset = Value;
}

FVector2 UBillBoardComp::GetUVScale() const
{
    return RenderData.Materials[0].UVScale;
}

FVector2 UBillBoardComp::GetUVOffset() const
{
    return RenderData.Materials[0].UVOffset;
}

void UBillBoardComp::UpdateWorldBounds()
{
    const FTransform GlobalTransform = GetGlobalTransform();
    const FVector Scale3D = GlobalTransform.GetScale3D();
    const float ScaleFactor = std::max(std::abs(Scale3D.Y), std::abs(Scale3D.Z));
    const float BaseRadius = 0.71f;
    const float WorldRadius = BaseRadius * ScaleFactor;

    WorldBounds.Center = GlobalTransform.GetLocation();
    WorldBounds.Extent = { WorldRadius, WorldRadius, WorldRadius };
    WorldBounds.Max = WorldBounds.Center + WorldBounds.Extent;
    WorldBounds.Min = WorldBounds.Center - WorldBounds.Extent;
}

FAxisAlignedBoundingBox UBillBoardComp::GetLocalBounds() const
{
    const float Radius = 0.71f; // (w 0.5, h 0.5)
    FAxisAlignedBoundingBox Box;
    Box.Center = { 0.0f, 0.0f, 0.0f };
    Box.Extent = { Radius, Radius, Radius };
    Box.Min = Box.Center - Box.Extent;
    Box.Max = Box.Center + Box.Extent;
    return Box;
}