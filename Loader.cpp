#include "Loader.h"
#include "D3DHook.h"
#include "Utils.h"
#include <dbghelp.h>
#include "api.h"
#include <d3d11_1.h>

template<>
struct fmt::formatter<GW2Load_HookedFunction> : fmt::formatter<std::string_view> {
    using Base = fmt::formatter<std::string_view>;

    template<class FmtContext>
    FmtContext::iterator format(GW2Load_HookedFunction hf, FmtContext& ctx) const
    {
        switch (hf)
        {
        case GW2Load_HookedFunction::Undefined:
            return Base::format("Undefined", ctx);
        case GW2Load_HookedFunction::Present:
            return Base::format("Present", ctx);
        case GW2Load_HookedFunction::ResizeBuffers:
            return Base::format("ResizeBuffers", ctx);
        default:
            assert(false);
            return std::format_to(ctx.out(), "<unknown {}>", std::to_underlying(hf));
        }
    }
};

template<>
struct fmt::formatter<GW2Load_CallbackPoint> : fmt::formatter<std::string_view> {
    using Base = fmt::formatter<std::string_view>;

    template<class FmtContext>
    FmtContext::iterator format(GW2Load_CallbackPoint hf, FmtContext& ctx) const
    {
        switch (hf)
        {
        case GW2Load_CallbackPoint::Undefined:
            return Base::format("Undefined", ctx);
        case GW2Load_CallbackPoint::BeforeCall:
            return Base::format("BeforeCall", ctx);
        case GW2Load_CallbackPoint::AfterCall:
            return Base::format("AfterCall", ctx);
        default:
            assert(false);
            return std::format_to(ctx.out(), "<unknown {}>", std::to_underlying(hf));
        }
    }
};

struct AddonData
{
    std::filesystem::path file;

    bool hasGetAddonDesc = false;
    bool hasOnLoad = false;
    bool hasOnLoadLauncher = false;
    bool hasOnClose = false;
    bool hasOnOutdated = false;

    HMODULE handle = nullptr;

    GW2Load_GetAddonDescription getAddonDesc = nullptr;
    GW2Load_OnLoad onLoad = nullptr;
    GW2Load_OnLoadLauncher onLoadLauncher = nullptr;
    GW2Load_OnClose onClose = nullptr;
    GW2Load_OnAddonDescriptionVersionOutdated onOutdated = nullptr;

    GW2Load_AddonDescription desc;
};

std::vector<AddonData> g_Addons;

BOOL CALLBACK EnumSymProc(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext)
{
    if ((pSymInfo->Flags & SYMFLAG_EXPORT) == 0)
        return true;

    auto& data = *static_cast<AddonData*>(UserContext);

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
        spdlog::critical("Could not acquire process handle: {}", GetLastErrorMessage());
        return;
    }
    Cleanup closeHandle{ [&] { CloseHandle(currentProcess); } };

    if (!SymInitialize(currentProcess, nullptr, false))
    {
        spdlog::critical("Could not initialize symbol handler: {}", GetLastErrorMessage());
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
                spdlog::warn("Could not load module {}: {}", file.path().string(), GetLastErrorMessage());
                continue;
            }

            AddonData data{ file.path() };

            if (!SymEnumSymbols(currentProcess, dllBase, "GW2Load_*", EnumSymProc, &data))
            {
                spdlog::warn("Could not enumerate symbols for {}: {}", data.file.string(), GetLastErrorMessage());
                continue;
            }

            if (!SymUnloadModule(currentProcess, dllBase))
                spdlog::warn("Could not unload module {}: {}", data.file.string(), GetLastErrorMessage());

            if (data.hasGetAddonDesc)
                g_Addons.push_back(std::move(data));
        }
    }
}

AddonData* g_CallbackAddon = nullptr;
void RegisterCallback(GW2Load_HookedFunction func, int priority, GW2Load_CallbackPoint callbackPoint, GW2Load_GenericCallback callback)
{
    spdlog::debug("Registering callback for {}: func={} priority={}, callbackPoint={}, callback={}",
        g_CallbackAddon->desc.name, func, priority, callbackPoint, fptr(callback));

    if (func == GW2Load_HookedFunction::Undefined || func >= GW2Load_HookedFunction::Count)
    {
        spdlog::error("Error when registering callback for {}: invalid function {}.", g_CallbackAddon->desc.name, func);
        return;
    }
    if (callbackPoint == GW2Load_CallbackPoint::Undefined || callbackPoint >= GW2Load_CallbackPoint::Count)
    {
        spdlog::error("Error when registering callback for {} at function {}: invalid callback point {}.", g_CallbackAddon->desc.name, func, callbackPoint);
        return;
    }
    if (callback == nullptr)
    {
        spdlog::error("Error when registering callback for {} at function {} point {}: null callback.", g_CallbackAddon->desc.name, func, callbackPoint);
        return;
    }

    g_Callbacks[GetIndex(func, callbackPoint)].emplace_back(priority, callback);
}

void InitializeAddons(bool launcher)
{
    GW2Load_API api{ RegisterCallback };

    for (auto& addon : g_Addons)
    {
        if (launcher && addon.hasOnLoadLauncher || !launcher && addon.hasOnLoad && !addon.hasOnLoadLauncher)
        {
            addon.handle = LoadLibrary(addon.file.c_str());
            if (addon.handle == nullptr)
            {
                spdlog::error("Addon {} could not be loaded: {}", addon.file.string(), GetLastErrorMessage());
                continue;
            }

            addon.getAddonDesc = reinterpret_cast<GW2Load_GetAddonDescription>(GetProcAddress(addon.handle, "GW2Load_GetAddonDescription"));
            if (addon.hasOnLoad)
                addon.onLoad = reinterpret_cast<GW2Load_OnLoad>(GetProcAddress(addon.handle, "GW2Load_OnLoad"));
            if (addon.hasOnLoadLauncher)
                addon.onLoadLauncher = reinterpret_cast<GW2Load_OnLoadLauncher>(GetProcAddress(addon.handle, "GW2Load_OnLoadLauncher"));
            if (addon.hasOnClose)
                addon.onClose = reinterpret_cast<GW2Load_OnClose>(GetProcAddress(addon.handle, "GW2Load_OnClose"));
            if (addon.hasOnOutdated)
                addon.onOutdated = reinterpret_cast<GW2Load_OnAddonDescriptionVersionOutdated>(GetProcAddress(addon.handle, "GW2Load_OnAddonDescriptionVersionOutdated"));

            if (!addon.getAddonDesc(&addon.desc))
            {
                spdlog::error("Addon {} refused to load, unloading...", addon.file.string());
                FreeLibrary(addon.handle);
                addon.handle = nullptr;
                continue;
            }

            switch (addon.desc.descriptionVersion)
            {
            case GW2Load_CurrentAddonDescriptionVersion:
                break;
            default:
            {
                if (addon.desc.descriptionVersion < GW2Load_CurrentAddonDescriptionVersion)
                {
                    spdlog::error("Addon {} uses API version {}, which is too old for current loader API version {}, unloading...",
                        addon.file.string(), addon.desc.descriptionVersion, GW2Load_CurrentAddonDescriptionVersion);
                    FreeLibrary(addon.handle);
                    addon.handle = nullptr;
                    continue;
                }
                else if(uint32_t addonVer = addon.desc.descriptionVersion; addon.onOutdated && addon.onOutdated(GW2Load_CurrentAddonDescriptionVersion, &addon.desc))
                {
                    spdlog::warn("Addon {} uses API version {}, which is newer than current loader API version {}; this is okay, as the addon supports backwards compatibility, but consider upgrading your loader.",
                        addon.file.string(), addonVer, GW2Load_CurrentAddonDescriptionVersion);
                }
                else
                {
                    spdlog::error("Addon {} uses API version {}, which is newer than current loader API version {}, unloading...",
                        addon.file.string(), addon.desc.descriptionVersion, GW2Load_CurrentAddonDescriptionVersion);
                    FreeLibrary(addon.handle);
                    addon.handle = nullptr;
                    continue;
                }
            }
            }

            spdlog::info("Addon {} recognized as {} v{}.{}.{}",
                addon.file.string(), addon.desc.name, addon.desc.majorAddonVersion, addon.desc.minorAddonVersion, addon.desc.patchAddonVersion);
        }

        if (launcher && addon.onLoadLauncher)
        {
            g_CallbackAddon = &addon;
            addon.onLoadLauncher(&api);
            g_CallbackAddon = nullptr;
            spdlog::debug("Addon {} OnLoadLauncher called.", addon.desc.name);
        }
        else if (!launcher && addon.onLoad)
        {
            g_CallbackAddon = &addon;
            addon.onLoad(&api, g_SwapChain, g_Device, g_DeviceContext);
            g_CallbackAddon = nullptr;
            spdlog::debug("Addon {} OnLoad called.", addon.desc.name);
        }
    }

    for (auto&& [i, cbs] : g_Callbacks)
    {
        std::ranges::sort(cbs, std::greater{}, &PriorityCallback::priority);
    }
}

void ShutdownAddons()
{
    for (auto& addon : g_Addons)
    {
        if (!addon.handle)
            continue;

        if (addon.onClose)
            addon.onClose();

        FreeLibrary(addon.handle);
        addon.handle = nullptr;
    }

    g_Addons.clear();
}

void Initialize(InitializationType type, std::optional<HWND> hwnd)
{
    switch (type)
    {
    case InitializationType::InLauncher:
        EnumerateAddons();
        InitializeAddons(true);
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
    ShutdownAddons();
    ShutdownD3DObjects(hwnd);
}