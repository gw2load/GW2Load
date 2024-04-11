#pragma once

#include <string_view>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <filesystem>
#include <memory>
#include <span>
#include <cwctype>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <spdlog/spdlog.h>