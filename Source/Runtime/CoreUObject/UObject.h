#pragma once
#include "UClass.h"
#include "Runtime/Core/IntTypes.h"
#include "ThirdParty/Json/json.hpp"
#include <cstddef>
#include <new>
#include <concepts>
//#include "Runtime/CoreUObject/UObjectGlobals.h"

class UObjectGlobals;
class UClass;
class FJsonArchive;
class FArchive;

/*
 * UObject를 상속받는 클래스는 반드시 GENERATED_BODY() 매크로를 사용해야 한다.
 * 또한 public 생성자를 만들면 안 된다.
 */
#define GENERATED_BODY()								\
	template <typename TObject, typename... TArgs>		\
		requires std::derived_from<TObject, UObject>	\
	friend TObject* NewObject(TArgs&&... Args);			\


 /*
  * 역직렬화를 위한 타입 등록 매크로.
  *
  * 파일에는 타입이 "Cube" 같은 문자열로만 남는다. C++에서는 문자열로
  * new 를 호출할 수 없으므로, 클래스마다 자기 자신을 생성하는 함수를
  * 미리 만들어 두고 이름과 짝지어 등록해 둔다. 로드할 때는 그 이름으로
  * 등록된 생성 함수를 찾아 호출한다.
  *
  * 사용법
  *   헤더 : 클래스 본문 안에 DECLARE_UCLASS(UCubeComp, UPrimitiveComponent)
  *   cpp  : 파일 어딘가에 IMPLEMENT_UCLASS(UCubeComp, UPrimitiveComponent)
  *
  * UObject 는 부모가 없어 ROOT 버전을 쓴다. 프로젝트에서 ROOT 버전은
  * UObject 한 곳에서만 사용한다.
  */
#define DECLARE_ROOT_UCLASS(ClassName)		\
	public:									\
		virtual UClass* GetClass() const;	\
		static UClass* StaticClass();		\
											\
	private:								\
		static UClass* ClassInfo;			\
		static UObject* CreateObject();		\


#define IMPLEMENT_ROOT_UCLASS(ClassName)																		\
UObject* ClassName::CreateObject()		{ return NewObject<ClassName>(); }										\
UClass* ClassName::ClassInfo			= UClass::RegisterToFactory(#ClassName, &ClassName::CreateObject, "");	\
UClass* ClassName::StaticClass()		{ return ClassInfo; }													\
UClass* ClassName::GetClass() const		{ return StaticClass(); }												\



#define DECLARE_UCLASS(ClassName, ParentClass)	\
public:											\
	UClass* GetClass() const override;			\
    static UClass* StaticClass();				\
    using Super = ParentClass;					\
												\
private:										\
    static UObject* CreateObject();				\
	static inline UClass* ClassInfo = UClass::RegisterToFactory(#ClassName, &ClassName::CreateObject, #ParentClass);	\

#define IMPLEMENT_UCLASS(ClassName, ParentClass)																			\
UObject* ClassName::CreateObject()		{ return NewObject<ClassName>(); }													\
UClass* ClassName::StaticClass()		{ return ClassInfo; }																\
UClass* ClassName::GetClass() const		{ return StaticClass(); }															\

#define UCLASS_META(ClassName, Key, Value)					\
struct _MetaRegister_##ClassName##_##Key					\
{															\
    _MetaRegister_##ClassName##_##Key()						\
	{														\
		ClassName::StaticClass()->SetMeta(#Key, Value);		\
	}														\
} _MetaRegisterInstance_##ClassName##_##Key;				\


class UObject
{
	GENERATED_BODY()
	DECLARE_ROOT_UCLASS(UObject)

	friend class FUObjectArray;

public:
	[[nodiscard]] uint32 GetUUID() const { return UUID; }

	UObject(const UObject&) = delete;
	UObject& operator=(const UObject&) = delete;

	UObject(UObject&&) = delete;
	const UObject& operator=(UObject&&) = delete;

	void SetUUID(uint32 _UUID) { UUID = _UUID; }

	virtual void Initialize();
	virtual void Release();
	virtual void PostInitProperties() {};

	// FArchive 공통 직렬화/역직렬화 함수. FArchive의 정책으로 데이터 및 참조를 처리함.
	virtual void Serialize(FArchive& Archive);

	static void* operator new(std::size_t Size);

	static void operator delete(void* Memory, std::size_t Size) noexcept;

	static void* operator new(std::size_t Size, std::align_val_t Alignment);

	static void operator delete(void* Memory, std::size_t Size, std::align_val_t Alignment) noexcept;

	static void* operator new[](std::size_t) = delete;
	static void operator delete[](void*) = delete;

	[[nodiscard]]
	static uint64 GetTotalAllocationBytes() {
		return TotalAllocationBytes;
	}

	[[nodiscard]]
	static uint64 GetTotalAllocationCount()
	{
		return TotalAllocationCount;
	}

protected:
	UObject() = default;
	virtual ~UObject() = default;

	virtual void Serialize(FJsonArchive& Archive) const;
	virtual void Deserialize(const FJsonArchive& Archive);



private:
	uint32 UUID = 0u;
	uint32 InternalIndex = 0u;

	static inline uint64 TotalAllocationBytes = 0;
	static inline uint64 TotalAllocationCount = 0;
public:
	template<typename T>
	bool IsA() const {
		return GetClass()->IsChildOrSelfOf(T::StaticClass());
	}

	bool IsA(UClass* ClassType) const
	{
		return GetClass()->IsChildOrSelfOf(ClassType);
	}

	template<typename T>
	T* Cast() {
		return IsA<T>() ? static_cast<T*>(this) : nullptr;
	}

	template<typename T>
	const T* Cast() const {
		return IsA<T>() ? static_cast<const T*>(this) : nullptr;
	}

	//Outer 관련 함수,변수들
public:
	UObject* GetOuter() const{ return Outer; }

	//부모중 가장 가까운 해당 타입의 객체를 리턴하는 함수
	template<typename T>
	T* GetTypedOuter()
	{
		for (UObject* Current = Outer; Current != nullptr; Current = Current->GetOuter())
		{
			if (T* Result = Current->Cast<T>()){ return Result; }
		}
		return nullptr;
	}
	//부모중 가장 가까운 해당 타입의 객체를 리턴하는 함수
	template<typename T>
	const T* GetTypedOuter() const
	{
		for (const UObject* Current = Outer; Current != nullptr; Current = Current->GetOuter())
		{
			if (const T* Result = Current->Cast<T>())
			{
				return Result;
			}
		}
		return nullptr;
	}

	bool IsIn(const UObject* Ancestor) const
	{
		if (!Ancestor){	return false; }
		for (const UObject* Current = Outer; Current != nullptr; Current = Current->GetOuter())
		{
			if (Current == Ancestor) { return true; }
		}
		return false;
	}

private:
	UObject* Outer = nullptr;

	// Outer 추가용 생성 헬퍼. 현재 구조를 바꾸고싶지않아서 이런형태임
	// TODO? : Outer가 필수가 되면 NewObject를 이 로직으로 바꾼다?
	template<typename TObject, typename... TArgs>
		requires std::derived_from<TObject, UObject>
	friend TObject* NewObjectWithOuter(UObject* InOuter,TArgs&&... Args);
	friend UObject* NewObjectWithOuter(UObject* InOuter, UClass* ClassType);


	//복사 정책 관련 함수
	UObject* Duplicate(UObject* Source) const;
};
