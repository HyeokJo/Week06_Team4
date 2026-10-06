#include "FDuplicateArchive.h"
#include "Runtime/Engine/UWorld.h"
#include "Runtime/Asset/UAsset.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include <cstdint>
#include <cstring>

FDuplicateArchive::FDuplicateArchive(bool bInLoading, TArray<uint8>& InData,
    TMap<const UObject*, UObject*>& InDuplicates, TArray<UObject*>& InPendingObjects,
    UWorld* InSourceWorld, EDuplicateFlags InFlags)
    : FArchive(bInLoading, true), Data(InData), Duplicates(InDuplicates),
    PendingObjects(InPendingObjects), SourceWorld(InSourceWorld), Flags(InFlags)
{
    // Writer와 Reader 모두 IsDuplicating()이 true다.
}

void FDuplicateArchive::SerializeBytes(void* Memory, size_t ByteCount)
{
    if (ByteCount == 0) return;

    if (IsLoading())
    {
        // 모든 실제 읽기의 범위 검사는 여기서 담당한다.
        if (ByteCount > Data.size() - Offset)
            throw std::runtime_error("Duplicate archive is truncated.");

        std::memcpy(Memory, Data.data() + Offset, ByteCount);
        Offset += ByteCount;
    }
    else
    {
        const uint8* Bytes = static_cast<const uint8*>(Memory);
        Data.insert(Data.end(), Bytes, Bytes + ByteCount);
    }
}

void FDuplicateArchive::CheckArraySize(uint32 Count, size_t MinimumElementBytes) const
{
    // resize 전에 최소한의 데이터가 남아 있는지 확인한다.
    if (IsLoading() && Count > (Data.size() - Offset) / MinimumElementBytes)
        throw std::runtime_error("Invalid duplicate array size.");
}

void FDuplicateArchive::CheckEnd() const
{
    // Writer와 Reader의 Serialize 호출 순서가 일치해야 한다.
    if (Offset != Data.size())
        throw std::runtime_error("Duplicate archive has unread data.");
}

bool FDuplicateArchive::BeginArray(const char*, uint32& Count)
{
    // 스코프 정보 대신 원소 개수만 기록한다.
    return SerializeNative(Count);
}

bool FDuplicateArchive::BeginMap(const char*, uint32& Count)
{
    // Map도 개수를 먼저 기록하고 키와 값을 순서대로 처리한다.
    return SerializeNative(Count);
}

void FDuplicateArchive::BeginMapEntry(uint32, FString& Key)
{
    // 기존 FArchive의 Map 순회가 키 다음에 값을 처리한다.
    SerializeValue(nullptr, Key);
}

bool FDuplicateArchive::SerializeValue(const char*, FString& Value)
{
    uint32 Count = IsSaving() ? ToCount(Value.size()) : 0;
    SerializeNative(Count);

    if (IsLoading())
    {
        // 문자열도 할당 전에 바이트 수를 확인한다.
        CheckArraySize(Count, sizeof(char));
        Value.resize(Count);
    }

    SerializeBytes(Value.data(), Count);
    return true;
}

UObject* FDuplicateArchive::GetOrCreateDuplicate(UObject* Source)
{
    if (const auto It = Duplicates.find(Source); It != Duplicates.end())
        return It->second;

    // Source는 원본 월드 내부 객체다. Outer의 복제본부터 확보한다.
    UObject* DuplicateOuter = GetOrCreateDuplicate(Source->GetOuter());
    UObject* Duplicate = nullptr;

    if (Source->IsA<AActor>())
    {
        ULevel* Level = DuplicateOuter->Cast<ULevel>();
        if (!Level) throw std::runtime_error("Actor Outer must be a Level.");

        AActor* Actor = Level->GetWorld()->SpawnActorDeferred(Source->GetClass());

        // 생성자의 기본 컴포넌트는 저장된 컴포넌트로 교체한다.
        Actor->DestroyOwnedComponents();
        Duplicate = Actor;
    }
    else if (Source->IsA<UActorComponent>())
    {
        AActor* Actor = DuplicateOuter->Cast<AActor>();
        if (!Actor) throw std::runtime_error("Component Outer must be an Actor.");

        // 인자 순서는 Outer, ClassType이다. 소유권만 먼저 연결한다.
        Duplicate = NewObjectWithOuter(Actor, Source->GetClass());
        Actor->AddComponent(Duplicate->Cast<UActorComponent>(), false);
    }
    else
    {
        // 현재 엔진은 World·Persistent Level·Actor·Component만 복제한다.
        throw std::runtime_error("Unsupported world subobject class.");
    }

    // Serialize 전에 대응표에 등록하므로 순환 참조도 재생성하지 않는다.
    Duplicates.emplace(Source, Duplicate);
    PendingObjects.push_back(Source);
    return Duplicate;
}

bool FDuplicateArchive::SerializeValue(const char*, UObject*& Value)
{
    // 주소는 이번 프로세스의 복제 작업 안에서만 사용하는 참조 토큰이다.
    uint64 Address = 0;

    if (IsSaving())
    {
        // 원본의 Value를 변경하지 않고 기록할 참조만 따로 결정한다.
        UObject* StoredObject = Value;

        //UAsset이 아닌경우 아래 처리
        if (StoredObject && !StoredObject->IsA<UAsset>())
        {
            if (StoredObject == SourceWorld || StoredObject->IsIn(SourceWorld))
            {
                GetOrCreateDuplicate(StoredObject);
            }
            else if ((static_cast<uint32>(Flags)
                & static_cast<uint32>(EDuplicateFlags::ShareExternalReferences)) == 0)
            {
                // 기본 정책에서는 월드 밖의 일반 객체와 연결하지 않는다.
                StoredObject = nullptr;
            }
        }

		// UAsset은 항상 원본 그대로 기록한다. 월드 밖의 일반 객체는 허용된 경우만 기록한다.

        Address = static_cast<uint64>(reinterpret_cast<std::uintptr_t>(StoredObject));
    }

    SerializeNative(Address);

    if (IsLoading())
    {
        UObject* Source = reinterpret_cast<UObject*>(static_cast<std::uintptr_t>(Address));
        const auto It = Duplicates.find(Source);

        // 월드 내부 참조는 복제본으로 치환하고, 허용된 공유 참조는 유지한다.
        Value = It != Duplicates.end() ? It->second : Source;
    }

    return true;
}