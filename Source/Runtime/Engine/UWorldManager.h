#pragma once
#include "UWorld.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"

class FCamera;

class UWorldManager final
{
public:
	void SaveWorld(const FString& path) const;
	void LoadWorld(const FString& path, FCamera* OutCamera = nullptr);
	void SetWorld(UWorld* World);
	void Release();

	UWorld* CurrentWorld = nullptr;
};
