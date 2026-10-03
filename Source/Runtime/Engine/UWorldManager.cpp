#include "UWorldManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include "ThirdParty/Json/json.hpp"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/CoreUObject/FUObjectArray.h"
#include "Runtime/Core/Log.h"

// 임시코드
#include "Converter.h"

void UWorldManager::SaveWorld(const FString& path) const
{
	std::filesystem::path fsPath(path);
	std::filesystem::path directory = fsPath.parent_path();

	if (!directory.empty() && !std::filesystem::exists(directory))
		std::filesystem::create_directories(directory);

	FUObjectArray& ObjectArray = FUObjectArray::Get();
	int32 UUID = ObjectArray.GetNextUUID();

	FArchive Archive;
	Archive.SetInt32("Version", 2);
	Archive.SetInt32("NextUUID", UUID);

	FArchive SceneArchive;
	CurrentWorld->Serialize(SceneArchive);
	Archive.SetArchive("World", SceneArchive);

	std::ofstream file(path);
	if (!file)
	{
		UE_LOG("[SaveWorld] 현재 씬을 파일로 저장하는데 실패했습니다. 파일에 쓸 수 없습니다.");
		return;
	}

	file << Archive.GetJSON().dump(4);
}

void UWorldManager::LoadWorld(const FString& path, FCamera* OutCamera)
{
	std::ifstream file(path);
	if (!file)
	{
		UE_LOG("[LoadWorld] 씬을 파일에서 불러오는데 실패했습니다. 파일을 읽을 수 없습니다.");
		return;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();

	nlohmann::json JSON = nlohmann::json::parse(buffer.str());
	FArchive Archive{ JSON };

	// TODO: TEMP: 경연 대회용 임시 컨버터 로직
	if (Archive.IsNull("Version") || Archive.GetInt32("Version") == 1)
	{
		Archive = Converter::GetStandardArchive(Archive, std::filesystem::path(path), OutCamera);
	}

	int32 Version = Archive.GetInt32("Version");
	if (Version != 2)
	{
		UE_LOG("[LoadWorld] 로드하려는 파일의 Scene Schema 버전이 다릅니다. 파일의 버전: %d, 지원하는 버전: %d", Version, 2);
		return;
	}

	int32 NextUUID = Archive.GetInt32("NextUUID");
	FUObjectArray& ObjectArray = FUObjectArray::Get();
	ObjectArray.SetNextUUID(NextUUID);

	if (Archive.IsNull("World"))
	{
		UE_LOG("[LoadWorld] 로드하려는 파일에서 World 항목이 없습니다. 파일 형식이 올바르지 않습니다.");
		return;
	}

	FArchive SceneArchive = Archive.GetArchive("World");

	UWorld* World = NewObject<UWorld>();
	World->Initialize();
	World->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());
	World->Deserialize(SceneArchive);

	SetWorld(World);
}

void UWorldManager::SetWorld(UWorld* World)
{
	if (World == nullptr) { return; }
	if (World == CurrentWorld) { return; }
	World->Initialize();
	World->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());

	if (CurrentWorld)
	{
		CurrentWorld->EndPlay();
		CurrentWorld->Deactivate();
		DestroyObject(CurrentWorld);
	}
	CurrentWorld = World;
	CurrentWorld->Activate();
	CurrentWorld->BeginPlay();
}

void UWorldManager::Release()
{
	if (CurrentWorld)
	{
		DestroyObject(CurrentWorld);
		CurrentWorld = nullptr;
	}
}
