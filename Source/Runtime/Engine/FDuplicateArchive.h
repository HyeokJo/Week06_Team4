#pragma once

#include "Runtime/Engine/FArchive.h"

class UWorld;
enum class EDuplicateFlags : uint32;

// 같은 클래스의 두 인스턴스를 Writer와 Reader로 사용한다.
class FDuplicateArchive final : public FArchive
{
public:
    FDuplicateArchive(bool bInLoading, TArray<uint8>& InData,
        TMap<const UObject*, UObject*>& InDuplicates, TArray<UObject*>& InPendingObjects,
        UWorld* InSourceWorld, EDuplicateFlags InFlags);

    // 메모리 버퍼에서는 필드 이름과 객체 스코프를 기록하지 않는다.
    bool BeginObject(const char*) override { return true; }
    void EndObject() override {}

    bool BeginArray(const char* Name, uint32& Count) override;
    void BeginArrayElement(uint32) override {}
    void EndArrayElement() override {}
    void EndArray() override {}

    bool BeginMap(const char* Name, uint32& Count) override;
    void BeginMapEntry(uint32 Index, FString& Key) override;
    void EndMapEntry() override {}
    void EndMap() override {}

    // 검증은 바이트 읽기와 컨테이너 할당 경계에 모은다.
    void CheckArraySize(uint32 Count, size_t MinimumElementBytes) const override;
    void CheckEnd() const;

protected:
    // 동일 프로세스 안에서 쓰고 읽으므로 기본 값은 바이트 그대로 전달한다.
    bool SerializeValue(const char*, int32& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, uint32& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, float& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, double& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, bool& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, FVector& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, FVector2& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char*, FVector4& Value) override { return SerializeNative(Value); }
    bool SerializeValue(const char* Name, FString& Value) override;
    bool SerializeValue(const char* Name, UObject*& Value) override;

private:
    template<typename T>
    bool SerializeNative(T& Value)
    {
        // UObject 전체를 memcpy하는 용도로 사용하지 않는다.
        static_assert(std::is_trivially_copyable_v<T>);
        SerializeBytes(&Value, sizeof(T));
        return true;
    }

    void SerializeBytes(void* Memory, size_t ByteCount);
    UObject* GetOrCreateDuplicate(UObject* Source);

    TArray<uint8>& Data;
    TMap<const UObject*, UObject*>& Duplicates;
    TArray<UObject*>& PendingObjects;
    UWorld* SourceWorld;
    EDuplicateFlags Flags;
    size_t Offset = 0;
};