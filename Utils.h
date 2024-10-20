#pragma once

#include "Common.h"

#include <MinHook.h>

struct LANGANDCODEPAGE
{
    WORD wLanguage;
    WORD wCodePage;
};

std::string_view GetWndProcMessageName(UINT msg);

const void* fptr(const auto p)
{
    return static_cast<const void*>(p);
}

constexpr std::size_t operator""_kb(unsigned long long int x) {
    return 1024ULL * x;
}

constexpr std::size_t operator""_mb(unsigned long long int x) {
    return 1024_kb * x;
}

constexpr std::size_t operator""_gb(unsigned long long int x) {
    return 1024_mb * x;
}

constexpr std::size_t operator""_tb(unsigned long long int x) {
    return 1024_gb * x;
}

constexpr std::size_t operator""_pb(unsigned long long int x) {
    return 1024_tb * x;
}

inline std::string ToLower(std::string in) {
    std::transform(in.begin(), in.end(), in.begin(), [](const char c) { return std::tolower(uint8_t(c)); });
    return in;
}
inline std::wstring ToLower(std::wstring in) {
    std::transform(in.begin(), in.end(), in.begin(), [](const wchar_t c) { return std::towlower(uint16_t(c)); });
    return in;
}

inline std::string ToUpper(std::string in) {
    std::transform(in.begin(), in.end(), in.begin(), [](const char c) { return std::toupper(uint8_t(c)); });
    return in;
}
inline std::wstring ToUpper(std::wstring in) {
    std::transform(in.begin(), in.end(), in.begin(), [](const wchar_t c) { return std::towupper(uint16_t(c)); });
    return in;
}

class Cleanup
{
    std::function<void()> callback_;

public:
    Cleanup(auto&& callback) : callback_(std::forward<decltype(callback)>(callback)) {}

    ~Cleanup() {
        callback_();
    }
};

const char* GetLastErrorMessage();

template <typename T>
inline tl::expected<T*, MH_STATUS> DetourLibraryFunction(HMODULE module, LPCSTR functionName, T* detour)
{
    auto address = GetProcAddress(module, functionName);
    if (!address)
        return tl::unexpected(MH_ERROR_FUNCTION_NOT_FOUND);

    T* original;
    auto status = MH_CreateHook(address, detour, reinterpret_cast<LPVOID*>(&original));
    if (status != MH_OK)
        return tl::unexpected(status);

    return original;
}

template<>
struct fmt::formatter<MH_STATUS> : fmt::formatter<int> {
    template<class FmtContext>
    FmtContext::iterator format(MH_STATUS status, FmtContext& ctx) const {
        return fmt::formatter<int>::format(static_cast<int>(status), ctx);
    }
};

template<typename T>
struct remove_const_recursive { using type = T; };

template<typename T>
struct remove_const_recursive<const volatile T> {
    using type = volatile typename remove_const_recursive<T>::type;
};

template<typename T>
struct remove_const_recursive<volatile T> {
    using type = volatile typename remove_const_recursive<T>::type;
};

template<typename T>
struct remove_const_recursive<const T> {
    using type = typename remove_const_recursive<T>::type;
};

template<typename T>
struct remove_const_recursive<T&> {
    using type = typename remove_const_recursive<T>::type&;
};

template<typename T>
struct remove_const_recursive<T*> {
    using type = typename remove_const_recursive<T>::type*;
};

auto& MutCast(auto& value)
{
    return const_cast<remove_const_recursive<decltype(value)>::type>(value);
}