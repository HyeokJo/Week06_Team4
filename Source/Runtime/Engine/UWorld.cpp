#include "UWorld.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/FArchive.h"
#include <algorithm>

#include "Runtime/CoreUObject/TObjectIterator.h"
#include "Runtime/Core/Log.h"

IMPLEMENT_UCLASS(UWorld, UObject)
UCLASS_META(UWorld, SerializeName, "World")

void UWorld::Initialize()
{
    if (bInitialized) {
        return;
    }
    
    Level = NewObject<ULevel>();
    Level->Initialize();
    Level->OwningWorld = this;

    Scene = new FScene();

    Super::Initialize();
    bInitialized = true;
}

void UWorld::Initialize(EWorldType InWorldType)
{
    Initialize();
    WorldType = InWorldType;
}

void UWorld::Release()
{
    if (bHasBegunPlay) {
        EndPlay();
    }
    if (bActive) {
        Deactivate();
    }

    Level->Release();
    Scene->Clear();
    
    bInitialized = false;

    Super::Release();
}

void UWorld::Activate()
{
    if (bActive) {
        return;
    }

    for (AActor* Actor : Level->Actors)
    {
        if (Actor)
            Actor->Register(*Level);
    }
    bActive = true;
}

void UWorld::Deactivate()
{
    if (!bActive)
        return;

    if (bHasBegunPlay)
        EndPlay();

    for (auto It = Level->Actors.rbegin(); It != Level->Actors.rend(); ++It)
    {
        if (*It)
            (*It)->Unregister();
    }
    bActive = false;
}

void UWorld::BeginPlay()
{
    if (!bActive || bHasBegunPlay)
    {
        return;
    }

    bHasBegunPlay = true;
    for (AActor* Actor : Level->Actors) 
    {
        if (Actor)
            Actor->BeginPlay();
    }
}

void UWorld::Update(float DeltaTime) 
{
    if (bHasBegunPlay) 
    {
        for (AActor* Actor : Level->Actors) 
        {
            if (Actor)
                Actor->Update(DeltaTime);
        }
    }
}

void UWorld::EndPlay()
{
    if (!bHasBegunPlay)
        return;

    for (auto It = Level->Actors.rbegin(); It != Level->Actors.rend(); ++It)
    {
        if (*It)
            (*It)->EndPlay();
    }
    bHasBegunPlay = false;
}

void UWorld::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);

    TArray<FArchive> ActorArchives;

    for (const auto& Item : Level->Actors)
    {
        if (!Item)
            continue;

        FArchive ItemArchive;
        Item->Serialize(ItemArchive);
        ActorArchives.push_back(ItemArchive);
    }

    Archive.SetArchiveArray("Actors", ActorArchives);
}

void UWorld::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);

    if (Archive.IsNull("Actors"))
    {
        // Actor 목록이 비어있음
        return;
    }

    TArray<FArchive> ActorArchives = Archive.GetArchiveArray("Actors");

    for (const auto& Item : ActorArchives)
    {
        UClass* ClassType = UClass::FindByName(Item.GetString("Type"));
        if (ClassType == nullptr)
            continue;

        AActor* Actor = SpawnActor(ClassType);
        if (!Actor)
            continue;

        Actor->Deserialize(Item);

        if (bActive)
            Actor->Register(*Level);

        if (bHasBegunPlay)
            Actor->BeginPlay();
    }
}

void UWorld::RemoveActor(AActor* Actor) { std::erase(Level->Actors, Actor); }

void UWorld::DestroyActor(AActor* Actor)
{
    if (Actor == nullptr)
        return;

    RemoveActor(Actor);
    DestroyObject(Actor);
}

AActor* UWorld::SpawnActor(UClass* ClassType)
{
    UObject* Object = NewObject(ClassType);
    AActor* Actor = Object->Cast<AActor>();
    if (!Actor)
    {
        DestroyObject(Object);
        return nullptr;
    }

    Actor->Initialize();
    Actor->Register(*Level);

    Level->Actors.push_back(Actor);

    return Actor;
}