#include "UWorld.h"
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
        Item->Serialize(ItemArchive);
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