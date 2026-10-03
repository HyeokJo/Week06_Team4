#include "ULevel.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Engine/FArchive.h"

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