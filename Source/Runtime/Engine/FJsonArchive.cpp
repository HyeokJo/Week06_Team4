#include "FJsonArchive.h"

#include "Runtime/Utility/WindowsUtil.h"
#include "Runtime/Asset/UAsset.h"
#include "Runtime/Asset/FAssetRegistry.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>


FJsonArchive::FJsonArchive()
	: FArchive(false), Object(nlohmann::json::object())
{
}


FJsonArchive::FJsonArchive(const nlohmann::json& InObject, const TMap<uint32, UObject*>* InObjects)
	: FArchive(true), Object(InObject), Objects(InObjects)
{
}

FJsonArchive::FJsonArchive(const FJsonArchive& Other)
	: FArchive(Other.IsLoading(), Other.IsDuplicating()),
	Object(Other.Object), Objects(Other.Objects)
{
	// 다른 인스턴스의 스코프 주소와 반복자는 복사하지 않는다.
}

// 데이터만 복사하는 대입연산자 오버로딩
FJsonArchive& FJsonArchive::operator=(const FJsonArchive& Other)
{
	if (this == &Other) return *this;

	// 데이터만 복사하고, 대상 Archive의 읽기·쓰기 모드는 유지한다.
	Object = Other.Object;
	Objects = Other.Objects;
	Scopes.clear();
	MapIterators.clear();
	return *this;
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
		Array.emplace_back(Item, Objects);
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
    return FJsonArchive(Object.at(Key), Objects);
}

void FJsonArchive::SetArchive(const FString& Key, const FJsonArchive& Archive)
{
	Object[Key] = Archive.GetJSON();
}

nlohmann::json* FJsonArchive::FindNode(const char* Name)
{
	// 이름이 없으면 현재 배열·Map 원소 자체를 처리한다.
	if (!Name) return &Current();

	nlohmann::json& Parent = Current();
	if (IsSaving()) return &Parent[Name];

	if (!Parent.is_object())
		throw std::runtime_error("Archive field parent must be an object.");

	auto It = Parent.find(Name);
	return It == Parent.end() ? nullptr : &It.value();
}

bool FJsonArchive::SerializeValue(const char* Name, FVector& Value)
{
    // 변환이 성공한 뒤에만 원래 값을 변경한다.
    std::array<float, 3> Parts{ Value.X, Value.Y, Value.Z };
    if (!TransferVector(Name, Parts)) return false;
    if (IsLoading()) Value = FVector{ Parts[0], Parts[1], Parts[2] };
    return true;
}

bool FJsonArchive::SerializeValue(const char* Name, FVector2& Value)
{
    // FVector2도 동일한 배열 표현을 사용한다.
    std::array<float, 2> Parts{ Value.X, Value.Y };
    if (!TransferVector(Name, Parts)) return false;
    if (IsLoading()) Value = FVector2{ Parts[0], Parts[1] };
    return true;
}

bool FJsonArchive::SerializeValue(const char* Name, FVector4& Value)
{
    // FVector4도 동일한 배열 표현을 사용한다.
    std::array<float, 4> Parts{ Value.X, Value.Y, Value.Z, Value.W };
    if (!TransferVector(Name, Parts)) return false;
    if (IsLoading()) Value = FVector4{ Parts[0], Parts[1], Parts[2], Parts[3] };
    return true;
}

bool FJsonArchive::SerializeValue(const char* Name, UObject*& Value)
{
    nlohmann::json* Node = FindNode(Name);
    if (!Node) return false;

    if (IsSaving())
    {
        // 주소 대신 null, 에셋 ID 문자열, 일반 객체의 저장 ID를 기록한다.
        if (!Value)
            *Node = nullptr;
        else if (UAsset* Asset = Value->Cast<UAsset>())
            *Node = Asset->GetID().ToString();
        else
            *Node = Value->GetUUID();
        return true;
    }

    if (Node->is_null())
    {
        Value = nullptr;
    }
    else if (Node->is_string())
    {
        // 에셋은 이미 로드된 Registry의 객체를 공유한다.
        UAsset* Asset = FAssetRegistry::GetInstance().Get<UAsset>(
            Node->get<FString>());
        if (!Asset)
            throw std::runtime_error("Archive asset was not found.");
        Value = Asset;
    }
    else
    {
        // 일반 객체는 로더가 생성한 객체 대응표에서 찾는다.
        if (!Objects)
            throw std::runtime_error("Archive object map is required.");
        Value = Objects->at(Node->get<uint32>());
    }
    return true;
}

bool FJsonArchive::BeginObject(const char* Name)
{
    nlohmann::json* Node = FindNode(Name);
    if (!Node) return false;

    // 성공했을 때만 스코프를 추가한다.
    if (IsSaving()) *Node = nlohmann::json::object();
    else if (!Node->is_object())
        throw std::runtime_error("Archive object expected.");

    Scopes.push_back(Node);
    return true;
}

void FJsonArchive::EndObject()
{
    // 성공한 BeginObject와 짝을 맞춘다.
    Scopes.pop_back();
}

bool FJsonArchive::BeginArray(const char* Name, uint32& Count)
{
    nlohmann::json* Node = FindNode(Name);
    if (!Node) return false;

    if (IsLoading())
    {
        if (!Node->is_array())
            throw std::runtime_error("Archive array expected.");
        Count = ToCount(Node->size());
    }
    else
    {
        // 원소 주소를 사용하기 전에 배열 크기를 확정한다.
        *Node = nlohmann::json::array();
        Node->get_ref<nlohmann::json::array_t&>().resize(Count);
    }
    Scopes.push_back(Node);
    return true;
}

void FJsonArchive::BeginArrayElement(uint32 Index)
{
    // 선택한 원소 안에서 Field(nullptr, Value)를 사용할 수 있다.
    Scopes.push_back(&Current().at(Index));
}

void FJsonArchive::EndArrayElement()
{
    // 현재 원소에서 배열 스코프로 돌아간다.
    Scopes.pop_back();
}

void FJsonArchive::EndArray()
{
    // 배열에서 부모 스코프로 돌아간다.
    Scopes.pop_back();
}

bool FJsonArchive::BeginMap(const char* Name, uint32& Count)
{
    // JSON Map은 문자열 키를 가진 JSON 객체로 표현한다.
    if (!BeginObject(Name)) return false;
    if (IsLoading()) Count = ToCount(Current().size());
    MapIterators.push_back(Current().begin());
    return true;
}

void FJsonArchive::BeginMapEntry(uint32 Index, FString& Key)
{
    // JSON에서는 Index 대신 키와 순차 반복자를 사용한다.
    if (IsLoading())
    {
        auto& It = MapIterators.back();
        Key = It.key();
        nlohmann::json* Node = &It.value();
        ++It;
        Scopes.push_back(Node);
    }
    else
    {
        Scopes.push_back(&Current()[Key]);
    }
}

void FJsonArchive::EndMapEntry()
{
    // 현재 값에서 Map 스코프로 돌아간다.
    Scopes.pop_back();
}

void FJsonArchive::EndMap()
{
    // 중첩 Map의 반복자와 스코프를 함께 정리한다.
    MapIterators.pop_back();
    EndObject();
}