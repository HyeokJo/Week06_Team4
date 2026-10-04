#pragma once

#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Math/FVector4.h"
#include "Runtime/Utility/WindowsUtil.h"
#include <concepts>
#include <type_traits>
#include <limits>
#include <stdexcept>
#include <cstddef>

/// <summary>
/// UObject의 데이터를 직렬화/역직렬화 하는 클래스입니다.
/// UObject의 데이터를 이 클래스에 담을 수도 있고, 이 데이터로 UObject를 만들 수도 있습니다.
/// UObject의 데이터를 담기 위해선 담을 형태에 맞는 클래스를 자식 클래스로 만들어 상속시켜야합니다.
/// </summary>
class FArchive
{
public:
    virtual ~FArchive() = default;

    // 읽기·쓰기 모드와 복제 여부는 생성 시 결정한다.
    bool IsLoading() const { return bLoading; }
    bool IsSaving() const { return !bLoading; }
    bool IsDuplicating() const { return bDuplicating; }

    template<typename T>
    void Field(const char* Name, T& Value)
    {
        // 필수 필드가 없으면 현재 읽기를 중단한다.
        if (!SerializeValue(Name, Value))
            throw std::runtime_error(FString("Missing Archive Field: ") + (Name ? Name : "<element>"));
    }

    template<typename T>
    bool OptionalField(const char* Name, T& Value)
    {
        // 없는 필드는 false를 반환하며 기존 값을 유지한다.
        return SerializeValue(Name, Value);
    }

    // Begin이 false를 반환했다면 스코프에 진입하지 않은 상태다.
    virtual bool BeginObject(const char* Name) = 0;
    virtual void EndObject() = 0;

    virtual bool BeginArray(const char* Name, uint32& Count) = 0;
    virtual void BeginArrayElement(uint32 Index) = 0;
    virtual void EndArrayElement() = 0;
    virtual void EndArray() = 0;

    virtual bool BeginMap(const char* Name, uint32& Count) = 0;
    virtual void BeginMapEntry(uint32 Index, FString& Key) = 0;
    virtual void EndMapEntry() = 0;
    virtual void EndMap() = 0;

    virtual void CheckArraySize(uint32 Count, size_t MinimumElementBytes) const
    {
        // 메모리 Reader에서 남은 바이트 수와 원소 개수를 검사한다.
    }

protected:
    explicit FArchive(bool bInLoading, bool bInDuplicating = false)
        : bLoading(bInLoading), bDuplicating(bInDuplicating) {
    }

    // 표현 방식은 JSON·메모리 Archive가 구현한다.
    virtual bool SerializeValue(const char* Name, int32& Value) = 0;
    virtual bool SerializeValue(const char* Name, uint32& Value) = 0;
    virtual bool SerializeValue(const char* Name, float& Value) = 0;
    virtual bool SerializeValue(const char* Name, double& Value) = 0;
    virtual bool SerializeValue(const char* Name, bool& Value) = 0;
    virtual bool SerializeValue(const char* Name, FString& Value) = 0;
    virtual bool SerializeValue(const char* Name, FVector& Value) = 0;
    virtual bool SerializeValue(const char* Name, FVector2& Value) = 0;
    virtual bool SerializeValue(const char* Name, FVector4& Value) = 0;

    // 객체 데이터 전체가 아니라 객체 참조를 처리한다.
    virtual bool SerializeValue(const char* Name, UObject*& Value) = 0;

    bool SerializeValue(const char* Name, FWString& Value)
    {
        // 문자열의 공통 표현은 UTF-8로 사용한다.
        FString Text = IsSaving() ? WindowsUtil::ToString(Value) : FString{};
        if (!SerializeValue(Name, Text)) return false;
        if (IsLoading()) Value = WindowsUtil::ToWString(Text);
        return true;
    }

    template<typename TEnum>
        requires std::is_enum_v<TEnum>
    bool SerializeValue(const char* Name, TEnum& Value)
    {
        // 현재 엔진의 Enum은 32비트 정수 표현으로 처리한다.
        using TUnderlying = std::underlying_type_t<TEnum>;
        using TStored = std::conditional_t<
            std::is_signed_v<TUnderlying>, int32, uint32>;

        static_assert(sizeof(TUnderlying) <= sizeof(TStored),
            "Enums larger than 32 bits are not supported.");

        TStored Number = static_cast<TStored>(Value);
        if (!SerializeValue(Name, Number)) return false;
        if (IsLoading()) Value = static_cast<TEnum>(Number);
        return true;
    }

    template<typename TObject>
        requires std::derived_from<TObject, UObject>
    bool SerializeValue(const char* Name, TObject*& Value)
    {
        // 파생 객체 포인터도 Archive의 객체 참조 정책을 적용한다.
        UObject* Object = Value;
        if (!SerializeValue(Name, Object)) return false;

        if (IsLoading())
        {
            TObject* TypedObject = Object ? Object->Cast<TObject>() : nullptr;
            if (Object && !TypedObject)
                throw std::runtime_error("Archive object type mismatch.");
            Value = TypedObject;
        }
        return true;
    }

    template<typename T>
    bool SerializeValue(const char* Name, TArray<T>& Values)
    {
        // std::vector<bool>은 일반적인 bool&를 반환하지 않는다.
        static_assert(!std::is_same_v<T, bool>,
            "TArray<bool> serialization is not supported.");

        uint32 Count = IsSaving() ? ToCount(Values.size()) : 0;
        if (!BeginArray(Name, Count)) return false;

        if (IsLoading())
        {
            // 고정 크기 숫자와 uint32 길이·참조 토큰을 사용하는 형식 기준.
            constexpr size_t MinimumBytes =
                std::is_arithmetic_v<T> ? sizeof(T) : sizeof(uint32);
            CheckArraySize(Count, MinimumBytes);
            Values.resize(Count);
        }

        for (uint32 Index = 0; Index < Count; ++Index)
        {
            BeginArrayElement(Index);
            Field(nullptr, Values[Index]);
            EndArrayElement();
        }
        EndArray();
        return true;
    }

    template<typename T>
    bool SerializeValue(const char* Name, TMap<FString, T>& Values)
    {
        // 문자열 키 Map도 별도 래핑 없이 기존 TMap을 사용한다.
        uint32 Count = IsSaving() ? ToCount(Values.size()) : 0;
        if (!BeginMap(Name, Count)) return false;

        if (IsLoading())
        {
            CheckArraySize(Count, sizeof(uint32));
            Values.clear();

            for (uint32 Index = 0; Index < Count; ++Index)
            {
                FString Key;
                T Value{};
                BeginMapEntry(Index, Key);
                Field(nullptr, Value);
                EndMapEntry();
                Values.emplace(std::move(Key), std::move(Value));
            }
        }
        else
        {
            uint32 Index = 0;
            for (auto& [Key, Value] : Values)
            {
                FString StoredKey = Key;
                BeginMapEntry(Index++, StoredKey);
                Field(nullptr, Value);
                EndMapEntry();
            }
        }
        EndMap();
        return true;
    }

    static uint32 ToCount(size_t Size)
    {
        // 크기를 uint32로 변환하기 전에 잘림을 방지한다.
        if (Size > (std::numeric_limits<uint32>::max)())
            throw std::length_error("Archive container is too large.");
        return static_cast<uint32>(Size);
    }

private:
    const bool bLoading;
    const bool bDuplicating;
};