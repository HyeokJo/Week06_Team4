#include "FWorldSerializer.h"
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include "ThirdParty/Json/json.hpp"
#include "Runtime/Engine/FJsonArchive.h"
#include "Runtime/CoreUObject/FUObjectArray.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
// 임시코드
#include "Converter.h"

void FWorldSerializer::SaveWorld(const FString& InPath, UWorld* InWorld)
{
	if (!InWorld) { return; }
	try
	{
		// 소유 목록으로 수집한다. 부착되지 않은 Component도 빠지지 않는다.
		TArray<UObject*> SavedObjects{ InWorld, InWorld->GetLevel() };
		for (AActor* Actor : InWorld->GetActors())
		{
			SavedObjects.push_back(Actor);
			for (UActorComponent* Component : Actor->GetAttachedComponents())
				SavedObjects.push_back(Component);
		}

		// 번호는 이번 파일 안에서만 유효하다. 런타임 UUID와는 무관하다.
		TMap<const UObject*, uint32> ObjectIndices;
		for (size_t Index = 0; Index < SavedObjects.size(); ++Index)
			ObjectIndices.emplace(SavedObjects[Index], static_cast<uint32>(Index + 1));

		nlohmann::json JSON;
		JSON["Version"] = 3;
		JSON["Objects"] = nlohmann::json::array();

		for (UObject* Object : SavedObjects)
		{
			// 최상위 World의 Outer는 파일 밖에 있으므로 0으로 기록한다.
			const uint32 OuterIndex =
				Object == InWorld ? 0 : ObjectIndices.at(Object->GetOuter());

			FJsonArchive Archive(&ObjectIndices);

			// 기존 JSON 오버로드 대신 공통 Archive 함수를 명시적으로 호출한다.
			Object->Serialize(static_cast<FArchive&>(Archive));
			JSON["Objects"].push_back({
				{ "Class", Object->GetClass()->GetUClassName() },
				{ "Outer", OuterIndex },
				{ "Data", Archive.GetJSON() }
				});
		}

		const std::filesystem::path Directory = std::filesystem::path(InPath).parent_path();
		if (!Directory.empty()) std::filesystem::create_directories(Directory);

		std::ofstream File(InPath);
		if (!File) throw std::runtime_error("Scene file cannot be opened for writing.");

		File << JSON.dump(4);
		if (!File) throw std::runtime_error("Scene file write failed.");
	}
	catch (const std::exception& Error)
	{
		// 저장 오류 보고는 파일 경계 한 곳에서 처리한다.
		UE_LOG("[SaveWorld] %s", Error.what());
	}
}

UWorld* FWorldSerializer::LoadWorld(const FString& InPath, FCamera* OutCamera)
{
	std::ifstream file(InPath);
	if (!file)
	{
		UE_LOG("[LoadWorld] 씬을 파일에서 불러오는데 실패했습니다. 파일을 읽을 수 없습니다.");
		return nullptr;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();

	nlohmann::json JSON = nlohmann::json::parse(buffer.str());
	FJsonArchive Archive{ JSON };

	// TODO: TEMP: 경연 대회용 임시 컨버터 로직
	if (Archive.IsNull("Version") || Archive.GetInt32("Version") == 1)
	{
		Archive = Converter::GetStandardArchive(Archive, std::filesystem::path(InPath), OutCamera);
	}

	int32 Version = Archive.GetInt32("Version");
	if (Version == 3)
	{
		return LoadWorld(Archive.GetJSON());
	}
	if (Version != 2 && Version != 3)
	{
		UE_LOG("[LoadWorld] 로드하려는 파일의 Scene Schema 버전이 다릅니다. 파일의 버전: %d, 지원하는 버전: %d, %d", Version, 2, 3);
		return nullptr;
	}
	// 아래의 기존 Version 2 검사와 로딩 코드는 유지한다.
	int32 NextUUID = Archive.GetInt32("NextUUID");
	FUObjectArray& ObjectArray = FUObjectArray::Get();
	ObjectArray.SetNextUUID(NextUUID);

	if (Archive.IsNull("World"))
	{
		UE_LOG("[LoadWorld] 로드하려는 파일에서 World 항목이 없습니다. 파일 형식이 올바르지 않습니다.");
		return nullptr;
	}

	FJsonArchive SceneArchive = Archive.GetArchive("World");

	UWorld* World = NewObject<UWorld>();
	World->Initialize();
	World->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());
	World->Deserialize(SceneArchive);

	return World;
}

UWorld* FWorldSerializer::LoadWorld(const nlohmann::json& JSON)
{
	UWorld* World = nullptr;

	try
	{
		const auto& Records = JSON.at("Objects");
		if (!Records.is_array() || Records.empty()
			|| Records.at(0).at("Class").get<FString>() != UWorld::StaticClass()->GetUClassName()
			|| Records.at(0).at("Outer").get<uint32>() != 0)
			throw std::runtime_error("Scene must start with a World record.");

		// 기존 Initialize가 Level과 FScene을 만든다. 저장된 Level에는 이 객체를 재사용한다.
		World = NewObject<UWorld>();
		World->Initialize();
		World->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());

		TMap<uint32, UObject*> Objects{ { 1, World } };
		bool bHasLevel = false;

		// 생성 단계: 먼저 생성된 Outer 아래에 객체를 만든다.
		for (size_t Index = 1; Index < Records.size(); ++Index)
		{
			const auto& Record = Records.at(Index);
			UClass* ClassType = UClass::FindByName(Record.at("Class").get<FString>());
			if (!ClassType) throw std::runtime_error("Unknown scene class.");

			UObject* Outer = Objects.at(Record.at("Outer").get<uint32>());
			UObject* Object = nullptr;

			if (ClassType == ULevel::StaticClass())
			{
				if (Outer != World || bHasLevel)
					throw std::runtime_error("Only one persistent Level is supported.");

				Object = World->GetLevel();
				bHasLevel = true;
			}
			else if (ClassType->IsChildOrSelfOf(AActor::StaticClass()))
			{
				if (Outer != World->GetLevel())
					throw std::runtime_error("Actor Outer must be the persistent Level.");

				AActor* Actor = World->SpawnActorDeferred(ClassType);

				// 생성자가 만든 기본 컴포넌트를 제거한 뒤 저장된 목록을 생성한다.
				Actor->DestroyOwnedComponents();
				Object = Actor;
			}
			else if (ClassType->IsChildOrSelfOf(UActorComponent::StaticClass()))
			{
				AActor* Actor = Outer->Cast<AActor>();
				if (!Actor) throw std::runtime_error("Component Outer must be an Actor.");

				// 기존 팩토리의 인자 순서는 Outer, ClassType이다.
				Object = NewObjectWithOuter(Actor, ClassType);

				// 소유권은 즉시 연결하지만 저장된 부착 관계는 아직 적용하지 않는다.
				Actor->AddComponent(Object->Cast<UActorComponent>(), false);
			}
			else
			{
				throw std::runtime_error("Unsupported scene object class.");
			}

			// 참조 번호는 1부터 시작한다. 아직 객체를 Initialize하지 않는다.
			Objects.emplace(static_cast<uint32>(Index + 1), Object);
		}

		if (!bHasLevel)
			throw std::runtime_error("Persistent Level record is missing.");

		// 전체 객체 생성 후 복원하므로 뒤에 저장된 객체도 즉시 참조할 수 있다.
		for (size_t Index = 0; Index < Records.size(); ++Index)
		{
			FJsonArchive Archive(Records.at(Index).at("Data"), &Objects);
			Objects.at(static_cast<uint32>(Index + 1))->Serialize(
				static_cast<FArchive&>(Archive));
		}

		// 데이터·참조 복원 후 캐시를 만든다. 이 월드는 아직 비활성이므로 등록되지 않는다.
		for (AActor* Actor : World->GetActors())
			World->FinishSpawningActor(Actor);

		return World;
	}
	catch (const std::exception& Error)
	{
		// 생성 즉시 소유 목록에 연결했으므로 실패한 월드 하나만 폐기한다.
		DestroyObject(World);
		UE_LOG("[LoadWorld] %s", Error.what());
		return nullptr;
	}
}