#pragma once

#include <string_view>

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

std::string_view GetWndProcMessageName(UINT msg);

bool InitializeHook(HWND hWnd);
void OverwriteVTables(void* sc, void* dev, void* ctx);