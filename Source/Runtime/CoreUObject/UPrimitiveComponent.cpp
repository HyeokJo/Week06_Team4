#include "UPrimitiveComponent.h"
#include "Runtime/Engine/FArchive.h"
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

void UPrimitiveComponent::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);

    // 에셋 참조의 표현은 Archive가 결정한다. JSON에서는 에셋 ID 문자열이다.
    Archive.OptionalField("Mesh", RenderData.Mesh);
    Archive.OptionalField("RenderType", RenderData.Type);

    uint32 Count = static_cast<uint32>(RenderData.Materials.size());
    if (!Archive.BeginArray("Materials", Count)) return;

    if (Archive.IsLoading())
    {
        // FMaterialInstance에는 기본 생성자가 없으므로 기존 기본 Material로 슬롯을 만든다.
        UMaterial* DefaultMaterial =
            FAssetRegistry::GetInstance().Get<UMaterial>("Material/Simple.json");

        RenderData.Materials.clear();
        RenderData.Materials.reserve(Count);
        for (uint32 Index = 0; Index < Count; ++Index)
            RenderData.Materials.emplace_back(DefaultMaterial);
    }

    for (uint32 Index = 0; Index < Count; ++Index)
    {
        Archive.BeginArrayElement(Index);
        FMaterialInstance& Material = RenderData.Materials[Index];

        // 슬롯의 값은 복사하고 Material·Pipeline·Texture 에셋은 공유한다.
        Archive.Field("Material", Material.Material);
        Archive.Field("Pipeline", Material.Pipeline);
        Archive.Field("Texture", Material.Texture);
        Archive.OptionalField("Color", Material.Color);
        Archive.OptionalField("UVOffset", Material.UVOffset);
        Archive.OptionalField("UVScale", Material.UVScale);
        Archive.OptionalField("DisableShading", Material.bDisableShading);
        Archive.OptionalField("Albedo", Material.Albedo);
        Archive.OptionalField("Diffuse", Material.Diffuse);
        Archive.OptionalField("Specular", Material.Specular);
        Archive.OptionalField("FilterMode", Material.SamplerDesc.FilterMode);
        Archive.OptionalField("WrapMode", Material.SamplerDesc.WrapMode);
        Archive.EndArrayElement();
    }
    Archive.EndArray();

    // SetMesh()는 Material 슬롯을 덮어쓸 수 있으므로 복원 중 호출하지 않는다.
    // 바운드, Material 캐시와 SortKey는 기존 Initialize()에서 재구성한다.
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
