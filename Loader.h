#pragma once

#include "Common.h"

enum class InitializationType
{
    InLauncher = 0,
    BeforeFirstWindow = 1,
    BeforeGameWindow = 2,
};

void Initialize(InitializationType type, std::optional<HWND> hwnd);
void InitializeAddons(bool launcher);
void Quit(HWND hwnd);