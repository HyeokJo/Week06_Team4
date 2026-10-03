#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Actors/AActor.h"

#include "ThirdParty/Json/json.hpp"

class UWorld;

class ULevel : public UObject
{
	DECLARE_UCLASS(ULevel, UObject)
	GENERATED_BODY()

	friend class UWorld;

public:
	void Initialize() override;
	void Release() override;

	UWorld* GetWorld() const { return OwningWorld; }

private:
	TArray<AActor*> Actors;
	UWorld* OwningWorld = nullptr;
};