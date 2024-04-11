#pragma once

#include "Common.h"

std::string_view GetWndProcMessageName(UINT msg);

const void* fptr(const auto* p)
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