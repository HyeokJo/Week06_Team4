#pragma once
#include "Runtime/Utility/EngineUtil.h"
#include "FArchive.h"
#include "ThirdParty/Json/json.hpp"
#include <array>



class FJsonArchive :public FArchive
{
public:
	FJsonArchive();
	explicit FJsonArchive(const TMap<const UObject*, uint32>* InObjectIndices);
	explicit FJsonArchive(const nlohmann::json& InObject, const TMap<uint32, UObject*>* InObjects = nullptr);
	FJsonArchive(const FJsonArchive& Other);
	FJsonArchive& operator=(const FJsonArchive& Other);

	nlohmann::json GetJSON() const { return Object; }

	int32 GetInt32(const FString& Key) const;
	void SetInt32(const FString& Key, int32 Value);

	float GetFloat(const FString& Key) const;
	void SetFloat(const FString& Key, float Value);

	uint32 GetUInt32(const FString& Key) const;
	void SetUInt32(const FString& Key, uint32 Value);

	double GetDouble(const FString& Key) const;
	void SetDouble(const FString& Key, double Value);

	bool GetBool(const FString& Key) const;
	void SetBool(const FString& Key, bool Value);

	FString GetString(const FString& Key) const;
	void SetString(const FString& Key, const FString& Value);

	FWString GetWString(const FString& Key) const;
	void SetWString(const FString& Key, const FWString& Value);

	bool IsNull(const FString& Key) const;
	void SetNull(const FString& Key);

	FVector GetVector(const FString& Key) const;
	void SetVector(const FString& Key, const FVector& Value);

	FVector2 GetVector2(const FString& Key) const;
	void SetVector2(const FString& Key, const FVector2& Value);

	FVector4 GetVector4(const FString& Key) const;
	void SetVector4(const FString& Key, const FVector4& Value);
	
	template <typename T>
	TArray<T> GetArray(const FString& Key) const;

	template <typename T>
	void SetArray(const FString& Key, const TArray<T>& Value);

	TArray<FJsonArchive> GetArchiveArray(const FString& Key) const;
	void SetArchiveArray(const FString& Key, const TArray<FJsonArchive>& Value);

	FJsonArchive GetArchive(const FString& Key) const;
	void SetArchive(const FString& Key, const FJsonArchive& Archive);

	template <typename T>
	T GetEnum(const FString& Key, TMap<FString, T>& EnumMap);

	template <typename T>
	void SetEnum(const FString& Key, T Value, TMap<T, FString>& EnumMap);

	bool BeginObject(const char* Name) override;
	void EndObject() override;

	bool BeginArray(const char* Name, uint32& Count) override;
	void BeginArrayElement(uint32 Index) override;
	void EndArrayElement() override;
	void EndArray() override;

	bool BeginMap(const char* Name, uint32& Count) override;
	void BeginMapEntry(uint32 Index, FString& Key) override;
	void EndMapEntry() override;
	void EndMap() override;


protected:
	// 숫자와 문자열은 같은 JSON 변환 함수를 사용한다.
	bool SerializeValue(const char* Name, int32& Value) override { return Transfer(Name, Value); }
	bool SerializeValue(const char* Name, uint32& Value) override { return Transfer(Name, Value); }
	bool SerializeValue(const char* Name, float& Value) override { return Transfer(Name, Value); }
	bool SerializeValue(const char* Name, double& Value) override { return Transfer(Name, Value); }
	bool SerializeValue(const char* Name, bool& Value) override { return Transfer(Name, Value); }
	bool SerializeValue(const char* Name, FString& Value) override { return Transfer(Name, Value); }

	bool SerializeValue(const char* Name, FVector& Value) override;
	bool SerializeValue(const char* Name, FVector2& Value) override;
	bool SerializeValue(const char* Name, FVector4& Value) override;
	bool SerializeValue(const char* Name, UObject*& Value) override;

private:
	nlohmann::json Object;
	TArray<nlohmann::json*> Scopes;
	TArray<nlohmann::json::iterator> MapIterators;

	// 객체 생성 코드가 제공하는 '저장 ID → 생성된 객체' 대응표.
	const TMap<uint32, UObject*>* Objects = nullptr;
	const TMap<const UObject*, uint32>* ObjectIndices = nullptr;
	nlohmann::json& Current()
	{
		return Scopes.empty() ? Object : *Scopes.back();
	}

	nlohmann::json* FindNode(const char* Name);

	template<typename T>
	bool Transfer(const char* Name, T& Value)
	{
		// 읽기에서 없는 키는 생성하지 않는다.
		nlohmann::json* Node = FindNode(Name);
		if (!Node) return false;

		if (IsLoading()) Value = Node->get<T>();
		else *Node = Value;
		return true;
	}

	template<size_t N>
	bool TransferVector(const char* Name, std::array<float, N>& Parts)
	{
		// 기존 벡터의 JSON 배열 표현을 유지한다.
		nlohmann::json* Node = FindNode(Name);
		if (!Node) return false;

		if (IsLoading())
		{
			if (!Node->is_array() || Node->size() != N)
				throw std::runtime_error("Invalid archive vector.");

			for (size_t Index = 0; Index < N; ++Index)
				Parts[Index] = Node->at(Index).get<float>();
		}
		else
		{
			*Node = Parts;
		}
		return true;
	}

};

template<typename T>
inline TArray<T> FJsonArchive::GetArray(const FString& Key) const
{
	TArray<T> Array;

	for (const auto& Item : Object.at(Key))
	{
		T Value = Item.get<T>();
		Array.push_back(Value);
	}

	return Array;
}

template<typename T>
inline void FJsonArchive::SetArray(const FString& Key, const TArray<T>& Value)
{
	Object[Key] = nlohmann::json::array();

	for (int i = 0; i < Value.size(); ++i)
	{
		Object[Key].push_back(Value[i]);
	}
}

template<typename T>
inline T FJsonArchive::GetEnum(const FString& Key, TMap<FString, T>& EnumMap)
{
	FString Value = GetString(Key);

	auto It = EnumMap.find(Value);
	if (It == EnumMap.end())
	{
		throw EngineUtil::CreateError("[FArchive::GetEnum] 키 {}에서 대해서 EnumMap에 없는 값이 있습니다. ({})", Key, Value);
	}

	return It->second;
}

template<typename T>
inline void FJsonArchive::SetEnum(const FString& Key, T Value, TMap<T, FString>& EnumMap)
{
	auto It = EnumMap.find(Value);

	if (It == EnumMap.end())
	{
		throw EngineUtil::CreateError(
			"[FArchive::SetEnum] 키 {}에서 대해서 EnumMap에 없는 값이 있습니다. ({})",
			Key, static_cast<int>(Value));
	}

	SetString(Key, It->second);
}
