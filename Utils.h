#pragma once

#include "Common.h"

struct LANGANDCODEPAGE
{
    WORD wLanguage;
    WORD wCodePage;
};

std::string_view GetWndProcMessageName(UINT msg);

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

template <>
struct fmt::formatter<spdlog::level::level_enum> : formatter<fmt::string_view> {
    // parse is inherited from formatter<string_view>.

    auto format(spdlog::level::level_enum level, format_context& ctx) const {
        auto name = spdlog::level::to_string_view(level);
        return formatter<fmt::string_view>::format(name, ctx);
    }
};
