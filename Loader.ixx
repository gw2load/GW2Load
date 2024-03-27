module;

#include "framework.h"

export module Loader;

import std;
import D3DHook;

export
{
    enum class InitializationType
    {
        InLauncher = 0,
        BeforeFirstWindow = 1,
        BeforeGameWindow = 2,
    };
    void Initialize(InitializationType type, std::optional<HWND> hwnd)
    {
        switch (type)
        {
        case InitializationType::InLauncher:
            break;
        case InitializationType::BeforeFirstWindow:
            break;
        case InitializationType::BeforeGameWindow:
            InitializeD3DHook(*hwnd);
            break;
        }
    }

    void Quit(HWND hwnd)
    {
        ShutdownD3DObjects(hwnd);
    }
}