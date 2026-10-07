#include "FAssetRegistry.h"
#include "Runtime/Utility/EngineUtil.h"

FAssetRegistry& FAssetRegistry::GetInstance()
{
	static FAssetRegistry Instance;
	return Instance;
}

void FAssetRegistry::Register(const FName& Name, UAsset* Pipeline)
{
	auto It = AssetMap.find(Name);
	if (It != AssetMap.end())
	{
		throw EngineUtil::CreateError("UAsset 등록 실패. 이미 중복된 이름({})이 존재합니다.", Name.ToString());
	}

	AssetMap.insert({ Name, Pipeline });
	DirectoryCache.clear();
}

void FAssetRegistry::Clear()
{
	AssetMap.clear();
	DirectoryCache.clear();
}
