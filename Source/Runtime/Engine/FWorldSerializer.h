#pragma once

#include "UWorld.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"

class FCamera;

class FWorldSerializer final
{
public:
	static void SaveWorld(const FString& InPath, UWorld* InWorld);
	static UWorld* LoadWorld(const FString& InPath, FCamera* OutCamera);
};