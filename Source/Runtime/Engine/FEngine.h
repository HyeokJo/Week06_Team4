#pragma once

#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Engine/UWorld.h"
#include "Editor/Application/IApplication.h"

class FEngineLoop;

// 엔진의 런타임 계층을 담당하는 클래스
// 엔진 로직, 시스템을 처리하는 부분은 여기서 담당
class FEngine
{
private:

	FRenderer Renderer;
	FRenderView RenderView{ Renderer };
	FEngineLoop& EngineLoop;
	TUniquePtr<IApplication> Application;
	TArray<UWorld*> WorldList; // TODO: World 기능이 더 믾아질 시 FWorldContext 구현 (멀티플레이, 네트워킹, Travel)

public:
	FEngine(FEngineLoop& InEngineLoop)
	    : EngineLoop{ InEngineLoop }
	{
	}

	void Init();
	void Tick(float DeltaTime);
	void Exit();

	void AddWorld(UWorld* World, EWorldType WorldType = EWorldType::Editor);
	void RemoveWorld(UWorld* World);
	UWorld* GetWorld(EWorldType Type) const;
	void UpdateWorldBounds() const;
};

extern FEngine* GEngine;