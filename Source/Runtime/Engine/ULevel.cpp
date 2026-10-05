#include "ULevel.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Engine/FJsonArchive.h"
#include <algorithm>

IMPLEMENT_UCLASS(ULevel, UObject)
UCLASS_META(ULevel, SerializeName, "Level")

void ULevel::Initialize()
{
	Super::Initialize();
}

void ULevel::Release()
{
    while (!Actors.empty()) 
    {
        AActor* Actor = Actors.back();
        Actors.pop_back();
        DestroyObject(Actor);
    }
}

void ULevel::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);

    // 생성 단계에서 확보한 Actor와 저장된 소유 목록이 같은지 여기서 확인한다.
    TArray<AActor*> StoredActors = Archive.IsSaving() ? Actors : TArray<AActor*>{};
    Archive.Field("Actors", StoredActors);

    if (Archive.IsLoading())
    {
        if (!std::is_permutation(
            StoredActors.begin(), StoredActors.end(), Actors.begin(), Actors.end()))
            throw std::runtime_error("Level actor ownership mismatch.");

        Actors = std::move(StoredActors);
    }

    // OwningWorld는 UWorld::Initialize()에서 연결하므로 중복 저장하지 않는다.
}