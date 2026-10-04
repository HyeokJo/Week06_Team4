#include "FJsonArchive.h"

#include "Runtime/Utility/WindowsUtil.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

FJsonArchive::FJsonArchive()
	: Object()
{
}

FJsonArchive::FJsonArchive(const nlohmann::json& InObject)
	: Object(InObject)
{
}

int32 FJsonArchive::GetInt32(const FString& Key) const
{
	return Object.at(Key).get<int32>();
}

void FJsonArchive::SetInt32(const FString& Key, int32 Value)
{
	Object[Key] = Value;
}

float FJsonArchive::GetFloat(const FString& Key) const
{
	return Object.at(Key).get<float>();
}

void FJsonArchive::SetFloat(const FString& Key, float Value)
{
	Object[Key] = Value;
}

uint32 FJsonArchive::GetUInt32(const FString& Key) const
{
	return Object.at(Key).get<uint32>();
}

void FJsonArchive::SetUInt32(const FString& Key, uint32 Value)
{
	Object[Key] = Value;
}

double FJsonArchive::GetDouble(const FString& Key) const
{
	return Object.at(Key).get<double>();
}

void FJsonArchive::SetDouble(const FString& Key, double Value)
{
	Object[Key] = Value;
}

bool FJsonArchive::GetBool(const FString& Key) const
{
	return Object.at(Key).get<bool>();
}

void FJsonArchive::SetBool(const FString& Key, bool Value)
{
	Object[Key] = Value;
}

FString FJsonArchive::GetString(const FString& Key) const
{
	return Object.at(Key).get<FString>();
}

void FJsonArchive::SetString(const FString& Key, const FString& Value)
{
	Object[Key] = Value;
}

FWString FJsonArchive::GetWString(const FString& Key) const
{
	FString Result = Object.at(Key).get<FString>();
	return WindowsUtil::ToWString(Result);
}

void FJsonArchive::SetWString(const FString& Key, const FWString& Value)
{
	FString Result = WindowsUtil::ToString(Value);
	Object[Key] = Result;
}

bool FJsonArchive::IsNull(const FString& Key) const
{
	// 주어진 키 자체가 존재하지 않음
	if (!Object.contains(Key)) { return true; }

	// 주어진 키의 value가 null 값임
	if (Object.at(Key).is_null()) { return true; }

	// 값이 있음
	return false;
}

void FJsonArchive::SetNull(const FString& Key)
{
	// 참고: IsNull과는 다르게, SetNull은 반드시 명시적인 null을 지정함
	Object[Key] = nullptr;
}

FVector FJsonArchive::GetVector(const FString& Key) const
{
	TArray<float> Array = GetArray<float>(Key);

	return FVector
	{
		Array[0],
		Array[1],
		Array[2],
	};
}

void FJsonArchive::SetVector(const FString& Key, const FVector& Value)
{
	TArray<float> Array
	{
		Value.X,
		Value.Y,
		Value.Z,
	};

	SetArray(Key, Array);
}

FVector2 FJsonArchive::GetVector2(const FString& Key) const
{
	TArray<float> Array = GetArray<float>(Key);

	return FVector2
	{
		Array[0],
		Array[1],
	};
}

void FJsonArchive::SetVector2(const FString& Key, const FVector2& Value)
{
	TArray<float> Array
	{
		Value.X,
		Value.Y,
	};

	SetArray(Key, Array);
}

FVector4 FJsonArchive::GetVector4(const FString& Key) const
{
	TArray<float> Array = GetArray<float>(Key);

	return FVector4
	{
		Array[0],
		Array[1],
		Array[2],
		Array[3],
	};
}

void FJsonArchive::SetVector4(const FString& Key, const FVector4& Value)
{
	TArray<float> Array
	{
		Value.X,
		Value.Y,
		Value.Z,
		Value.W,
	};

	SetArray(Key, Array);
}

TArray<FJsonArchive> FJsonArchive::GetArchiveArray(const FString& Key) const
{
	TArray<FJsonArchive> Array;

	for (const auto& Item : Object.at(Key))
	{
		FJsonArchive ItemArchive{ Item };
		Array.push_back(ItemArchive);
	}

	return Array;
}

void FJsonArchive::SetArchiveArray(const FString& Key, const TArray<FJsonArchive>& Value)
{
	Object[Key] = nlohmann::json::array();

	for (const auto& Item : Value)
	{
		Object.at(Key).push_back(Item.GetJSON());
	}
}

FJsonArchive FJsonArchive::GetArchive(const FString& Key) const
{
	return FJsonArchive{ Object.at(Key) };
}

void FJsonArchive::SetArchive(const FString& Key, const FJsonArchive& Archive)
{
	Object[Key] = Archive.GetJSON();
}
