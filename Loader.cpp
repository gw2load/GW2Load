#include "Loader.h"
#include "D3DHook.h"
#include "Utils.h"
#include <dbghelp.h>

std::vector<ModuleSymbolData> g_AddonSignatures;

struct ModuleSymbolData
{
    std::filesystem::path file;
    bool hasGetAddonDesc = false;
    bool hasOnLoad = false;
    bool hasOnLoadLauncher = false;
    bool hasOnClose = false;
    bool hasOnOutdated = false;
};

BOOL CALLBACK EnumSymProc(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext)
{
    if ((pSymInfo->Flags & SYMFLAG_EXPORT) == 0)
        return true;

    auto& data = *static_cast<ModuleSymbolData*>(UserContext);

    std::string_view name{ pSymInfo->Name, pSymInfo->NameLen };

    if (name == "GW2Load_GetAddonDescription")
        data.hasGetAddonDesc = true;
    else if (name == "GW2Load_OnLoad")
        data.hasOnLoad = true;
    else if (name == "GW2Load_OnLoadLauncher")
        data.hasOnLoadLauncher = true;
    else if (name == "GW2Load_OnClose")
        data.hasOnClose = true;
    else if (name == "GW2Load_OnAddonDescriptionVersionOutdated")
        data.hasOnOutdated = true;

    return true;
}

void EnumerateAddons()
{
    HANDLE currentProcess;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &currentProcess, 0, false, DUPLICATE_SAME_ACCESS))
    {
        spdlog::critical("Could not acquire process handle: {}", GetLastError());
        return;
    }
    Cleanup closeHandle{ [&] { CloseHandle(currentProcess); } };

    if (!SymInitialize(currentProcess, nullptr, false))
    {
        spdlog::critical("Could not initialize symbol handler: {}", GetLastError());
        return;
    }
    Cleanup symbolCleanup{ [&] { SymCleanup(currentProcess); } };

    for (const auto& dir : std::filesystem::directory_iterator{ "addons" })
    {
        if (!dir.is_directory()) continue;
        auto dirName = dir.path().filename().string();
        if (dirName.starts_with(".") || dirName.starts_with("_")) continue;

        for (const auto& file : std::filesystem::directory_iterator{ dir.path() })
        {
            if (!file.is_regular_file()) continue;
            if (!file.path().has_extension() || ToLower(file.path().extension().string()) != ".dll") continue;

            auto dllBase = SymLoadModuleExW(currentProcess, nullptr, file.path().c_str(), nullptr, 0, 0, nullptr, 0);
            if (dllBase == 0)
            {
                spdlog::warn("Could not load module {}: {}", file.path().string(), GetLastError());
                continue;
            }

            ModuleSymbolData data{ file.path() };

            if (!SymEnumSymbols(currentProcess, dllBase, "GW2Load_*", EnumSymProc, &data))
            {
                spdlog::warn("Could not enumerate symbols for {}: {}", data.file.string(), GetLastError());
                continue;
            }

            if (!SymUnloadModule(currentProcess, dllBase))
                spdlog::warn("Could not unload module {}: {}", data.file.string(), GetLastError());

            if (data.hasGetAddonDesc)
                g_AddonSignatures.push_back(std::move(data));
        }
    }
}

void Initialize(InitializationType type, std::optional<HWND> hwnd)
{
    switch (type)
    {
    case InitializationType::InLauncher:
        EnumerateAddons();
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