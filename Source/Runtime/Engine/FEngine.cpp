#include "FEngine.h"

#include "Runtime/Core/Log.h"
#include "Runtime/Core/Globals.h"
#include "Runtime/Core/FMemory.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Engine/FEngineLoop.h"
#include "Runtime/Engine/FTimeManager.h"
#include "Runtime/Engine/UWorld.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Resource/FResourceLoader.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Utility/EngineUtil.h"

#include "Editor/Application/FEditorApplication.h"
#include "Editor/Application/FObjViewerApplication.h"

#include <Windows.h>

FEngine* GEngine = nullptr;

void FEngine::Init()
{
	GEngine = this;

	HWND Window = EngineLoop.GetMainWindowHandle();
	if (!Renderer.Initialize(Window))
	{
		throw EngineUtil::CreateError("FRenderer 초기화에 실패했습니다.");
	}
	FStatsManager::Get().Initialize(Renderer.GetDevice());
	FMemory::Init();

	FRenderResourceLibrary& RenderResources = FRenderResourceLibrary::Get();
	if (!RenderResources.Initialize(Renderer))
	{
		throw EngineUtil::CreateError("FRenderResourceLibrary 초기화에 실패했습니다.");
	}

	UClass::ResolveTypeBitsets();

	FResourceLoader::LoadAssets();

#if defined(_OBJVIEWER)
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;
	Renderer.GetDeviceAndContext_ImplDX11(Device, Context);

	TUniquePtr<FObjViewerApplication> ObjViewer = MakeUnique<FObjViewerApplication>(Renderer);
	ObjViewer->Initialize(Window, Device, Context);
	Application = std::move(ObjViewer);

#else
	// 새씬 생성
	AddWorld(NewObject<UWorld>());

	TUniquePtr<FEditorApplication> EditorApp = MakeUnique<FEditorApplication>();
	{
		ID3D11Device* Device = nullptr;
		ID3D11DeviceContext* Context = nullptr;
		Renderer.GetDeviceAndContext_ImplDX11(Device, Context);
		EditorApp->Initialize_ImguiWin32DX11(Window, Device, Context);
	}
	EditorApp->Initialize_Runtime(&RenderView);
	Application = std::move(EditorApp);
#endif
}

void FEngine::Tick(float DeltaTime)
{
	FStatsManager::Get().ResetFrame();
	FInputManager::Get().BeginFrame();
	
	if (Globals::bIsRequestingResize)
	{
		Renderer.OnWindowSize(Globals::ResizeWidth, Globals::ResizeHeight);
		Application->OnWindowSize(Globals::ResizeWidth, Globals::ResizeHeight);
		Globals::bIsRequestingResize = false;
	}

	{
		SCOPE_CYCLE_COUNTER("Game");

		// TODO: PIE Update / Editor Update 구분
		for (auto& World : WorldList)
		{
			if (World)
				World->Update(DeltaTime);
		}

		Application->Update(DeltaTime);
	}

	{
		SCOPE_CYCLE_COUNTER("Draw");
		Renderer.BeginFrame();
		Application->Render();
		Renderer.SwapBuffer();
	}

	SET_CYCLE_COUNTER("Frame", FTimeManager::GetDeltaTime() * 1000.0f);

	FInputManager::Get().EndFrame();

	// 입력 메시지 수신 ~ 프레임 종료까지의 지연. 프레임의 맨 마지막이어야 한다.
	FInputLatencyTimer::Get().Tick();
}

void FEngine::Exit()
{
	Application->Shutdown();

#if !defined(_OBJVIEWER)
	for (auto& World : WorldList)
	{
		if (World)
		{
			DestroyObject(World);
		}
	}
	WorldList.clear();
#endif
	Application.Reset();

	Renderer.Shutdown();

	GEngine = nullptr;
}

void FEngine::AddWorld(UWorld* World, EWorldType WorldType)
{
	if (World == nullptr) { return; }

	if (WorldType == EWorldType::Editor)
	{
		UWorld* EditorWorld = GetWorld(EWorldType::Editor);
		if (EditorWorld)
		{
			// 존재하던 Editor 타입 월드는 제거
			RemoveWorld(EditorWorld);
		}
	}

	World->Initialize(WorldType);
	World->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());
	// 등록, 활성화, 비긴플레이 내에서도 새 월드를 조회가능하도록 하는 순서 개편
	WorldList.push_back(World);
	World->Activate();

	if (WorldType == EWorldType::PIE || WorldType == EWorldType::Game)
		World->BeginPlay();


	// 로드된 컴포넌트는 대기열에만 쌓이므로, 트랜스폼이 모두 설정된 지금 트리를 만든다.
	World->GetSceneBVH().Build(World->GetRenderComponents());
}

void FEngine::RemoveWorld(UWorld* InWorld)
{
	if (InWorld == nullptr) { return; }

	int Index = -1;

	for (int i = 0; i < WorldList.size(); ++i)
	{
		if (WorldList[i] == InWorld)
		{
			Index = i;
			break;
		}
	}

	if (Index >= 0)
	{
		std::swap(WorldList[Index], WorldList.back());
		WorldList.pop_back();
	}

	InWorld->EndPlay();
	InWorld->Deactivate();
	DestroyObject(InWorld);
}

UWorld* FEngine::GetWorld(EWorldType Type) const
{
	for (auto& World : WorldList)
	{
		if (World->GetWorldType() == Type)
			return World;
	}

	return nullptr;
}

void FEngine::UpdateWorldBounds() const
{
	for (auto& World : WorldList)
	{
		if (World)
			World->UpdateDirtyBounds();
	}
}