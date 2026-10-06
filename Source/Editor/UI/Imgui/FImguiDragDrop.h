#pragma once

#include "Runtime/Asset/UAsset.h";

struct FContentDragPayload
{
    UAsset* Ptr;
};

inline constexpr const char* ContentDragPayloadType = "ENGINE_CONTENT";

inline constexpr const char* ComponentClassDragPayloadType = "ENGINE_COMPONENT_CLASS";