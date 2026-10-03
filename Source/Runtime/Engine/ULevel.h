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

private:
	TArray<AActor*> Actors;
};