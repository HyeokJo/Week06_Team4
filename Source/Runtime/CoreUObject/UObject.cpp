#include "UObject.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Engine/FJsonArchive.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Core/FMemory.h"

IMPLEMENT_ROOT_UCLASS(UObject)
UCLASS_META(UObject, DisplayName, "Object")

void UObject::Initialize()
{
}

void UObject::Release()
{
}

void UObject::Serialize(FJsonArchive& Archive) const
{
	Archive.SetInt32("UUID", UUID);
	Archive.SetString("Type", GetClass()->GetUClassName());
}

void UObject::Deserialize(const FJsonArchive& Archive)
{
	UUID = Archive.GetInt32("UUID");
}
void UObject::Serialize(FArchive& Archive)
{
	// UUID는 팩토리가 새로 부여한다. Class와 Outer는 객체 목록에서 처리한다.
	// 공통 저장 데이터가 추가되면 이곳에 Field를 추가한다.
}
void* UObject::operator new(std::size_t Size)
{
	// void* Memory = ::operator new(Size);
	
	void* Memory = FMemory::Malloc(Size);
	
	TotalAllocationBytes += Size;
	++TotalAllocationCount;

	return Memory;
}

void UObject::operator delete(void* Memory, std::size_t Size) noexcept
{
	if (Memory == nullptr) return;

	TotalAllocationBytes -= Size;
	--TotalAllocationCount;

	//::operator delete(Memory);

	FMemory::Free(Memory);
}

void* UObject::operator new(std::size_t Size, std::align_val_t Alignment)
{
	// void* Memory = ::operator new(Size, Alignment);
	void* Memory = FMemory::Malloc(Size, static_cast<size_t>(Alignment));

	TotalAllocationBytes += Size;
	++TotalAllocationCount;

	return Memory;
}

void UObject::operator delete(void* Memory, std::size_t Size, std::align_val_t Alignment) noexcept
{
	if (Memory == nullptr) return;

	TotalAllocationBytes -= Size;
	--TotalAllocationCount;

	//::operator delete(Memory, Alignment);
	FMemory::Free(Memory, static_cast<size_t>(Alignment));
}
