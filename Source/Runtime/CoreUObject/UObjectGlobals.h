#pragma once

#include "FUObjectArray.h"
#include <concepts>

// TODO: 참조를 확실하게 관리하려면 TObjectPtr<TObject>를 반환하도록 바꿔야 함
template <typename TObject, typename ... TArgs>
	requires std::derived_from<TObject, UObject>
TObject* NewObject(TArgs&&... Args)
{
	TObject* Object = new TObject(std::forward<TArgs>(Args)...);
	try {
		FUObjectArray::Get().AddObject(Object);
	}
	catch (...) {
		delete Object;
		throw;
	}
	try
	{
		static_cast<UObject*>(Object)->PostInitProperties();
	}
	catch (...)
	{
		// 등록 이후 실패했다면 객체 배열에서도 제거해야 한다.
		FUObjectArray::Get().DestroyObject(Object);
		throw;
	}

	return Object;
}

inline UObject* NewObject(UClass* ClassType)
{
	UObject* Object = ClassType->CreateDefaultObject();
	return Object;
}

template<typename TObject, typename... TArgs>
    requires std::derived_from<TObject, UObject>
TObject* NewObjectWithOuter(UObject* InOuter, TArgs&&... Args)
{
    TObject* Object = NewObject<TObject>(std::forward<TArgs>(Args)...);
    static_cast<UObject*>(Object)->Outer = InOuter;
    return Object;
}

inline UObject* NewObjectWithOuter(UObject* InOuter, UClass* ClassType)
{
    if (!ClassType){ return nullptr; }
    UObject* Object = NewObject(ClassType);
	if (Object) { Object->Outer = InOuter; }
    return Object;
}

/// <summary>
/// UObject를 엔진에서 안전하게 할당 해제합니다. (delete Object와 동일)
/// 제거된 UObject 포인터는 반드시 폐기해주세요.
/// </summary>
/// <param name="Object"></param>
inline void DestroyObject(UObject* Object)
{
	FUObjectArray& ObjectArray = FUObjectArray::Get();
	ObjectArray.DestroyObject(Object);
}