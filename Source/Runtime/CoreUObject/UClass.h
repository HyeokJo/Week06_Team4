#pragma once
#include "Runtime/Core/TFunction.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/CoreUObject/FClassIdSet.h"

class UObject;

class UClass
{
private:
	static inline uint32 registeredCount = 0;
	static inline TArray<TUniquePtr<UClass>> classList;
	static inline TMap<FString, uint32> nameToId;
	static inline TMap<FString, uint32> displayNameToId;
	FString className, superClassTypeName;
	TFunction<UObject* ()> createFunction;
	uint32 typeId;
	UClass* superClass;
	TMap<FString, FString> metadata;
	FClassIdSet classIdSet;
	bool processed = false;

public:

	UObject* CreateDefaultObject() const; 
	static UClass* RegisterToFactory(
		const FString& typeName,
		const TFunction<UObject* ()>& createFunction, 
		const FString& superClassTypeName);

	static UClass* FindByName(const FString& Name);
	const FString& GetDisplayName() const;
	void SetMeta(const FString& key, const FString& value);

	// TODO: 드롭다운 리스트를 만들기 위한 임시 게터 함수, 리플렉션 시스템 제작후 변경 가능하면 좋을듯.
	static uint32 GetRegisteredClassCount()
	{
		return static_cast<uint32>(classList.size());
	}
	bool HasMetaValue(const FString& Key, const FString& Value) const
	{
		const auto It = metadata.find(Key);
		return It != metadata.end() && It->second == Value;
	}

	static void ResolveTypeBitsets();
	void ResolveTypeBitset(UClass* classPtr);

	bool IsChildOrSelfOf(UClass* baseClass) const;

	[[nodiscard]] const FString& GetUClassName() const { return className; }

	static UClass* FindClassWithDisplayName(const FString& name)
	{

		auto it = displayNameToId.find(name);
		if (it != displayNameToId.end())
			return GetClassById(it->second);

		it = nameToId.find(name);
		return (it != nameToId.end()) ? GetClassById(it->second) : nullptr;
	}

	static UClass* GetClassById(uint32 typeId) {
		return (typeId < classList.size()) ? classList[typeId].get() : nullptr;
	}

	UClass* GetSuperClass()
	{
		return superClass;
	}





};