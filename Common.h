#pragma once

#include <string_view>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <memory>
#include <span>
#include <cwctype>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <spdlog/spdlog.h>

#include "api.h"

enum class CallbackIndex : unsigned int {};

constexpr CallbackIndex GetIndex(GW2Load_HookedFunction function, GW2Load_CallbackPoint point)
{
    return static_cast<CallbackIndex>((std::to_underlying(function) << 4) | std::to_underlying(point));
}

struct PriorityCallback
{
    int priority;
    GW2Load_GenericCallback callback;
};
static std::unordered_map<CallbackIndex, std::vector<PriorityCallback>> g_Callbacks;

template<GW2Load_HookedFunction Function, GW2Load_CallbackPoint Point, typename... Args>
void InvokeAPIHooks(Args&& ...args)
{
    for (auto&& [priority, cb] : g_Callbacks[GetIndex(Function, Point)])
        reinterpret_cast<void(*)(Args&&...)>(cb)(std::forward<Args>(args)...);
}