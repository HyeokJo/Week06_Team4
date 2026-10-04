#include "UPrimitiveComponent.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Asset/UStaticMesh.h"
#include "Runtime/Engine/UWorld.h"
#include "Runtime/Engine/ULevel.h"

IMPLEMENT_UCLASS(UPrimitiveComponent, USceneComponent)

void UPrimitiveComponent::PostInitProperties()
{
    Super::PostInitProperties();

    // 생성 직후 기본 렌더 종류와 Material 슬롯을 준비한다.
    RenderData.Type = ERenderType::Primitive;
    FAssetRegistry& Registry = FAssetRegistry::GetInstance();
    RenderData.Materials.emplace_back(Registry.Get<UMaterial>("Material/Simple.json"));
}

void UPrimitiveComponent::Initialize()
{
    if (IsInitialized()) return;
    Super::Initialize();

    // 기본값 또는 복원된 최종 메시로 로컬 바운드를 다시 계산한다.
    LocalBounds = RenderData.Mesh ? RenderData.Mesh->Get()->GetLocalBounds() : FAxisAlignedBoundingBox{};

    // Material을 새로 지정하지 않고 현재 데이터로 캐시를 만든다.
    UpdateMaterialCache();
    UpdateSortKey();
    MarkBoundDirty();
}

void UPrimitiveComponent::SetMesh(UStaticMesh* Mesh)
{
    RenderData.Mesh = Mesh;
    LocalBounds = Mesh->Get()->GetLocalBounds();
    MarkBoundDirty();
}

void UPrimitiveComponent::SetMaterial(UMaterial* Material, int32 Index)
{
    if (!Material || Index < 0) { return; }

    const size_t TargetIndex = static_cast<size_t>(Index);
    if (RenderData.Materials.size() <= TargetIndex)
    {
        RenderData.Materials.resize(TargetIndex + 1, FMaterialInstance{ Material });
    }
    RenderData.Materials[TargetIndex] = FMaterialInstance{ Material };
    UpdateMaterialCache();
    UpdateSortKey();
}

void UPrimitiveComponent::SetTexture(UTexture* Texture, int32 Index)
{
    if (Index < 0 || static_cast<size_t>(Index) >= RenderData.Materials.size()) { return; }
    RenderData.Materials[static_cast<size_t>(Index)].Texture = Texture;
    UpdateMaterialCache();
    UpdateSortKey();
}

void UPrimitiveComponent::SetColor(const FVector4& Color, int32 Index)
{
    if (Index < 0 || static_cast<size_t>(Index) >= RenderData.Materials.size()) { return; }
    RenderData.Materials[static_cast<size_t>(Index)].Color = Color;
}

void UPrimitiveComponent::MarkBoundDirty()
{
    if (Level)
    {
        Level->GetWorld()->MarkBoundsDirty(this);
    }
    else
    {
        // 레벨에 등록하지 않고 직접 그리는 에디터 오버레이 등.
        UpdateWorldBounds();
    }

}

void UPrimitiveComponent::UpdateWorldBounds()
{
    // 유효하지 않은 LocalBounds를 변환하면 잘못된 유효 박스가 생길 수 있다.
    const FAxisAlignedBoundingBox Local = GetLocalBounds();
    WorldBounds = Local.IsValid()
        ? FAxisAlignedBoundingBox(Local, GetGlobalTransformMatrix())
        : FAxisAlignedBoundingBox{};
}

void UPrimitiveComponent::OnTransformChanged()
{
    MarkBoundDirty();
}

void UPrimitiveComponent::SetRelativeTransform(const FTransform& RelativeTransform)
{
    Super::SetRelativeTransform(RelativeTransform);
}

const FAxisAlignedBoundingBox &UPrimitiveComponent::GetWorldBounds() const
{
    return WorldBounds;
}

FAxisAlignedBoundingBox UPrimitiveComponent::GetViewBounds(const FCamera& Camera) const
{
    return FAxisAlignedBoundingBox(GetWorldBounds(), Camera.GetViewMatrix());
}

void UPrimitiveComponent::Register(ULevel& InLevel)
{
    if (RenderData.Type == ERenderType::None)
    {
        RenderData.Type = ERenderType::Primitive;
    }

    Super::Register(InLevel);
    InLevel.GetWorld()->AddRenderComponent(this);
}

void UPrimitiveComponent::Unregister()
{
    if (Level)
    {
        Level->GetWorld()->RemoveRenderComponent(this);
    }
    Super::Unregister();
}

void UPrimitiveComponent::UpdateMaterialCache()
{
	CachedMaterials.clear();
	CachedMaterials.reserve(RenderData.Materials.size());
	
	for (const auto& Item : RenderData.Materials)
	{
		if (!Item.Pipeline)
		{
			continue;
		}

		FMaterial Material{};
		Material.SetPipeLine(Item.Pipeline->Get());

		if (Item.Texture)
		{
			Material.SetTexture(Item.Texture->Get());
		}
		Material.SetSamplerDesc(Item.SamplerDesc);
		CachedMaterials.push_back(Material);
	}
}

bool UPrimitiveComponent::IsOcclusionTarget() const
{
    return RenderData.Mesh != nullptr && RenderData.Mesh->Get() != nullptr;
}

void UPrimitiveComponent::UpdateSortKey()
{
    RenderData.SortKey = 0;
    if (RenderData.Materials.empty())
    {
        return;
    }

    const FMaterialInstance& Material = RenderData.Materials[0];
    const uint64 PipelineId = Material.Pipeline
        ? static_cast<uint64>(Material.Pipeline->GetID().GetHash())
        : 0;
    const uint64 MaterialId = Material.Material
        ? static_cast<uint64>(Material.Material->GetID().GetHash())
        : 0;
    const uint64 TextureId = Material.Texture
        ? static_cast<uint64>(Material.Texture->GetID().GetHash())
        : 0;

    RenderData.SortKey =
        ((PipelineId & 0xFFFFull) << 48) |
        ((MaterialId & 0xFFFFull) << 32) |
        ((TextureId & 0xFFFFull) << 16);
}
