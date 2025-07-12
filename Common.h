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
#include <ranges>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <spdlog/spdlog.h>

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include "api.h"

constexpr unsigned int PrintDescVersion(int v)
{
    return v & ~GW2Load_AddonAPIVersionMagicFlag;
}

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

struct CallbackElement
{
	std::mutex lock;
    std::vector<PriorityCallback> callbacks;
};
extern std::unordered_map<CallbackIndex, CallbackElement> g_Callbacks;

template<GW2Load_HookedFunction Function, GW2Load_CallbackPoint Point, typename CallbackType, typename... Args>
void InvokeAPIHooks(Args&& ...args)
{
    constexpr auto idx = GetIndex(Function, Point);
    auto& callbacks = g_Callbacks[idx];

	std::lock_guard guard(callbacks.lock);
    for (auto&& [priority, cb] : callbacks.callbacks) {
        reinterpret_cast<CallbackType>(cb)(std::forward<Args>(args)...);
    }
}

bool IsAttachedToGame();