#pragma once
#include "Core.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/TArray.h"
#include <algorithm>
#include <cstddef>


enum class ETickGroup : uint32
{
    Update,
    PostUpdate,
    Count
};

constexpr uint32 TickGroupCount = static_cast<uint32>(ETickGroup::Count);

struct FTickSettings
{
    bool bCanEverTick = false;
    bool bTickEnabled = false;
    ETickGroup TickGroup = ETickGroup::Update;

    // 등록 상태는 목록 관리 코드만 변경합니다. Count는 미등록입니다.
    ETickGroup RegisteredGroup = ETickGroup::Count;
};
template<typename T>
class TTickRegistry
{
public:
    // Register/Unregister 또는 Tick 설정 변경 시에만 목록을 갱신한다.
    void Refresh(T* Object, FTickSettings& Settings, bool bRegistered)
    {
        const bool bValidGroup = static_cast<uint32>(Settings.TickGroup) < TickGroupCount;
        const ETickGroup DesiredGroup =
            bRegistered && Settings.bCanEverTick && Settings.bTickEnabled && bValidGroup
            ? Settings.TickGroup : ETickGroup::Count;
        if (Settings.RegisteredGroup == DesiredGroup) return;

        if (Settings.RegisteredGroup != ETickGroup::Count)
        {
            const uint32 GroupIndex = static_cast<uint32>(Settings.RegisteredGroup);
            TArray<T*>& List = Groups[GroupIndex];
            const auto It = std::find(List.begin(), List.end(), Object);
            if (It != List.end())
            {
                // 실행 중에는 인덱스를 유지해 뒤의 객체를 건너뛰지 않는다.
                if (bTicking)
                {
                    *It = nullptr;
                    bNeedsCompact[GroupIndex] = true;
                }
                else
                {
                    List.erase(It);
                }
            }
        }

        Settings.RegisteredGroup = DesiredGroup;
        if (DesiredGroup != ETickGroup::Count)
            Groups[static_cast<uint32>(DesiredGroup)].push_back(Object);
    }

    void BeginTick()
    {
        // 모든 그룹의 범위를 미리 고정한다. 새 등록은 다음 프레임부터 실행한다.
        bTicking = true;
        for (uint32 Index = 0; Index < TickGroupCount; ++Index)
            TickCounts[Index] = Groups[Index].size();
    }

    void TickGroup(ETickGroup Group, float DeltaTime, bool bEditorWorld)
    {
        const uint32 GroupIndex = static_cast<uint32>(Group);
        TArray<T*>& List = Groups[GroupIndex];
        for (std::size_t Index = 0; Index < TickCounts[GroupIndex]; ++Index)
        {
            // Update 중 배열이 재할당될 수 있으므로 원소 참조를 보관하지 않는다.
            T* Object = List[Index];
            if (Object && Object->ShouldTick(bEditorWorld))
                Object->Update(DeltaTime);
        }
    }

    void EndTick()
    {
        // 모든 그룹 실행 후 제거된 자리만 정리한다.
        bTicking = false;
        for (uint32 Index = 0; Index < TickGroupCount; ++Index)
        {
            if (!bNeedsCompact[Index]) continue;
            std::erase(Groups[Index], static_cast<T*>(nullptr));
            bNeedsCompact[Index] = false;
        }
    }

    void Clear()
    {
        // 모든 대상의 Unregister가 끝난 뒤에만 호출한다.
        for (uint32 Index = 0; Index < TickGroupCount; ++Index)
        {
            Groups[Index].clear();
            TickCounts[Index] = 0;
            bNeedsCompact[Index] = false;
        }
        bTicking = false;
    }

private:
    TArray<T*> Groups[TickGroupCount];
    std::size_t TickCounts[TickGroupCount]{};
    bool bNeedsCompact[TickGroupCount]{};
    bool bTicking = false;
};