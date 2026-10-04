#pragma once

#include "Runtime/Actors/AActor.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "FScene.h"
#include "ULevel.h"

#include <concepts>
#include <type_traits>

#include "ThirdParty/Json/json.hpp"

enum EWorldType
{
	Editor, EditorPreview, PIE, Game
};

class UWorld : public UObject
{
	DECLARE_UCLASS(UWorld, UObject)
	GENERATED_BODY()

public:
    void Initialize() override;
    void Initialize(EWorldType InWorldType);
    void Release() override;
    void Activate();
    void Deactivate();
    void BeginPlay();
    void Update(float DeltaTime);
    void EndPlay();

    [[nodiscard]] bool IsActive() const { return bActive; }
    [[nodiscard]] bool HasBegunPlay() const { return bHasBegunPlay; }

    // 액터 목록 반환
    [[nodiscard]] const TArray<AActor*>& GetActors() const { return Level->Actors; }

    // 위치와 크기를 지정하여 액터 생성
    template <typename TActor, typename... TArgs>
        requires std::derived_from<TActor, AActor>
    TActor* SpawnActor(const FVector& Location, const FVector& Scale, TArgs &&...Args) 
    {
        TActor* Actor = SpawnActorDeferred<TActor>(std::forward<TArgs>(Args)...);

        if (USceneComponent* Root = Actor->GetRootComponent())
        {
            FTransform Transform{};
            Transform.SetLocation(Location);
            Transform.SetScale3D(Scale);
            Root->SetRelativeTransform(Transform);
        }

        FinishSpawningActor(Actor);
        return Actor;
    }

    // 기본 위치와 크기로 액터 생성
    template <typename TActor>
        requires std::derived_from<TActor, AActor>
    TActor* SpawnActor() 
    {
        return SpawnActor<TActor>(FVector(0.0f, 0.0f, 0.0f),
            FVector(1.0f, 1.0f, 1.0f));
    }

    // 첫번째 인자가 벡터가 아닐 때 기본 위치와 크기 전달
    template <typename TActor, typename FirstArg, typename... RestArgs>
        requires std::derived_from<TActor, AActor> &&
    (!std::is_same_v<std::decay_t<FirstArg>, FVector>)
    TActor* SpawnActor(FirstArg&& First, RestArgs &&...Rest) 
    {
        return SpawnActor<TActor>(
            FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f),
            std::forward<FirstArg>(First), std::forward<RestArgs>(Rest)...
        );
    }
    
    //Actor 지연 생성/생성완료 관련 함수들.
    // 클래스 타입으로 생성하되 Initialize·Register·BeginPlay는 미룬다.
    AActor* SpawnActorDeferred(UClass* ClassType);

    // Transform 설정이나 데이터 복원이 끝난 Actor를 실행 가능한 상태로 만든다.
    void FinishSpawningActor(AActor* Actor);

    template<typename TActor, typename... TArgs>
        requires std::derived_from<TActor, AActor>
    TActor* SpawnActorDeferred(TArgs&&... Args)
    {
        // 팩토리의 PostInitProperties까지만 실행된 상태다.
        TActor* Actor = NewObjectWithOuter<TActor>(
            Level, std::forward<TArgs>(Args)...);

        Level->Actors.push_back(Actor);
        return Actor;
    }

    virtual void Serialize(FArchive& Archive) const override;
    virtual void Deserialize(const FArchive& Archive) override;

    void SetRenderResourceLibrary(FRenderResourceLibrary* InRenderResourceLibrary) { return Scene->SetRenderResourceLibrary(InRenderResourceLibrary); }
    [[nodiscard]] const TArray<UPrimitiveComponent*>& GetRenderComponents() const { return Scene->GetRenderComponents(); }
    [[nodiscard]] const TArray<FAxisAlignedBoundingBox>& GetCullDataList() const { return Scene->GetCullDataList(); }
    void MarkBoundsDirty(UPrimitiveComponent* Prim) { Scene->MarkBoundsDirty(Prim); }
    void UpdateDirtyBounds() { Scene->UpdateDirtyBounds(); }
    void AddRenderComponent(UPrimitiveComponent* Prim) { Scene->AddRenderComponent(Prim); }
    void RemoveRenderComponent(UPrimitiveComponent* Prim) { Scene->RemoveRenderComponent(Prim); }
    FSceneBVH& GetSceneBVH() { return Scene->GetSceneBVH(); }
    const FSceneBVH& GetSceneBVH() const { return Scene->GetSceneBVH(); }
    [[nodiscard]] const TArray<uint8>& GetOcclusionTargetFlags() const { return Scene->GetOcclusionTargetFlags(); }

    void RemoveActor(AActor* Actor);
    void DestroyActor(AActor* Actor);

    AActor* SpawnActor(UClass* ClassType);

    const EWorldType GetWorldType() const { return WorldType; }

private:
    FScene* Scene = nullptr;
    ULevel* Level = nullptr;// persistent level
	// TODO: SubLevel 지원 필요
    EWorldType WorldType = EWorldType::Editor;

    bool bInitialized = false;
    bool bActive = false;
    bool bHasBegunPlay = false;
};