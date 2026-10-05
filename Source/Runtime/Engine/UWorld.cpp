#include "UWorld.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/FJsonArchive.h"
#include <algorithm>

#include "Runtime/CoreUObject/TObjectIterator.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Engine/FDuplicateArchive.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"

IMPLEMENT_UCLASS(UWorld, UObject)
UCLASS_META(UWorld, SerializeName, "World")

void UWorld::Initialize()
{
    if (bInitialized) {
        return;
    }
    
    Level = NewObjectWithOuter<ULevel>(this);
    Level->OwningWorld = this;
    Level->Initialize();
    
    Scene = new FScene();
    Super::Initialize();
    bInitialized = true;
}

void UWorld::Initialize(EWorldType InWorldType)
{
    WorldType = InWorldType;    //WorldType이 있어야하는지 없어야하는지 판단이 서지않음.    
    // PIE를 만들때 월드 타입이 있으면 훨씬 편하게 만들 수 있지 않을까?하는 생각이 있음.
    Initialize();
}

void UWorld::Release()
{
    if (bHasBegunPlay) {
        EndPlay();
    }
    if (bActive) {
        Deactivate();
    }
	DestroyObject(Level);   //여기서 Level->Release()가 호출됨
    Level = nullptr;
    if (Scene)
    {
        Scene->Clear();
        delete Scene;
        Scene = nullptr;
    }

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
        if (Actor && Actor->IsInitialized())
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
    // PIE나 Game일때만 BeginPlay.
    if (WorldType != EWorldType::PIE && WorldType != EWorldType::Game)
        return;

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
    if (!bHasBegunPlay) { return; }
    
    for (AActor* Actor : Level->Actors) 
    {
        if (Actor && Actor->HasBegunPlay() && Actor->IsActorTickEnabled())
            Actor->Update(DeltaTime);
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

void UWorld::Serialize(FJsonArchive& Archive) const
{
    Super::Serialize(Archive);

    TArray<FJsonArchive> ActorArchives;

    for (const auto& Item : Level->Actors)
    {
        if (!Item)
            continue;

        FJsonArchive ItemArchive;
        static_cast<const AActor*>(Item)->Serialize(ItemArchive);
        ActorArchives.push_back(ItemArchive);
    }

    Archive.SetArchiveArray("Actors", ActorArchives);
}

void UWorld::Deserialize(const FJsonArchive& Archive)
{
    Super::Deserialize(Archive);

    if (Archive.IsNull("Actors"))
    {
        // Actor 목록이 비어있음
        return;
    }
    const size_t FirstNewActor = Level->Actors.size();
    TArray<FJsonArchive> ActorArchives = Archive.GetArchiveArray("Actors");

    for (const auto& Item : ActorArchives)
    {
        UClass* ClassType = UClass::FindByName(Item.GetString("Type"));
        if (ClassType == nullptr) continue;

        AActor* Actor = SpawnActorDeferred(ClassType);
        if (!Actor) continue;

        Actor->Deserialize(Item);
    }
    // Deserialize 후 지연 생성된 액터들에 대해 생성 완료 처리.
    for (size_t Index = FirstNewActor; Index < Level->Actors.size(); ++Index)
        FinishSpawningActor(Level->Actors[Index]);

}

void UWorld::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);

    // 현재 프로젝트는 Persistent Level 하나만 사용한다.
    ULevel* StoredLevel = Level;
    Archive.Field("Level", StoredLevel);
    if (Archive.IsLoading() && StoredLevel != Level)
        throw std::runtime_error("World level reference mismatch.");

    // WorldType, FScene, 활성화·BeginPlay 상태는 실행 환경에서 다시 구성한다.
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
    AActor* Actor = SpawnActorDeferred(ClassType);
    FinishSpawningActor(Actor);
    return Actor;
}

AActor* UWorld::SpawnActorDeferred(UClass* ClassType)
{
    // 동적 클래스 생성에서도 Outer가 먼저인 기존 인자 순서를 사용한다.
    UObject* Object = NewObjectWithOuter(Level, ClassType);
    if (!Object) return nullptr;

    AActor* Actor = Object->Cast<AActor>();
    if (!Actor)
    {
        DestroyObject(Object);
        return nullptr;
    }

    Level->Actors.push_back(Actor);
    return Actor;
}

void UWorld::FinishSpawningActor(AActor* Actor)
{
    if (!Actor) return;

    // 최종 데이터로 컴포넌트 캐시와 실행 상태를 준비한다.
    Actor->Initialize();

    // 비활성 월드의 로딩 중에는 아직 등록하지 않는다.
    if (bActive)
        Actor->Register(*Level);

    // 이미 플레이 중인 월드에 생성한 Actor만 바로 플레이를 시작한다.
    if (bHasBegunPlay)
        Actor->BeginPlay();
}

UWorld* UWorld::DuplicateWorld(UWorld* SourceWorld, EWorldType TargetWorldType,
    EDuplicateFlags Flags, TMap<const UObject*, UObject*>* OutDuplicates)
{
    if (!SourceWorld || !SourceWorld->GetLevel()) return nullptr;
    UWorld* NewWorld = nullptr;

    try
    {
        NewWorld = NewObject<UWorld>();

        // PIE로 고정하지 않고 호출자가 지정한 종류로 초기화한다.
        NewWorld->Initialize(TargetWorldType);
        NewWorld->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());

        // 새 World와 Initialize가 만든 Level을 원본에 대응시킨다.
        TMap<const UObject*, UObject*> Duplicates{
            { SourceWorld, NewWorld },
            { SourceWorld->GetLevel(), NewWorld->GetLevel() }
        };
        TArray<UObject*> PendingObjects{ SourceWorld, SourceWorld->GetLevel() };
        TArray<uint8> Data;

        // Writer가 내부 참조를 발견하면서 복제본을 생성하고 데이터를 기록한다.
        FDuplicateArchive Writer(false, Data, Duplicates, PendingObjects, SourceWorld, Flags);
        for (size_t Index = 0; Index < PendingObjects.size(); ++Index)
        {
            UObject* Source = PendingObjects[Index];
            Source->Serialize(Writer);
        }

        // 같은 대응표를 사용해 데이터를 복원하고 내부 참조를 치환한다.
        FDuplicateArchive Reader(true, Data, Duplicates, PendingObjects, SourceWorld, Flags);
        for (UObject* Source : PendingObjects)
            Duplicates.at(Source)->Serialize(Reader);
        Reader.CheckEnd();

        // 복원 완료 후 초기화한다. 등록·BeginPlay는 호출자가 별도로 진행한다.
        for (AActor* Actor : NewWorld->GetActors())
            NewWorld->FinishSpawningActor(Actor);

        // 필요한 경우 호출자가 대응표를 보관한다.
        if (OutDuplicates) *OutDuplicates = std::move(Duplicates);
        return NewWorld;
    }
    catch (const std::exception& Error)
    {
        // 실패하면 이번에 생성한 월드만 폐기한다.
        DestroyObject(NewWorld);
        UE_LOG("[DuplicateWorld] %s", Error.what());
        return nullptr;
    }
}