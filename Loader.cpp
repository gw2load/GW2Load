#include "Loader.h"
#include "D3DHook.h"
#include "Utils.h"
#include <dbghelp.h>
#include "api.h"
#include <d3d11_1.h>
#include <future>

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
    bool hasUpdateCheck = false;

    HMODULE handle = nullptr;

    GW2Load_GetAddonDescription getAddonDesc = nullptr;
    GW2Load_OnLoad onLoad = nullptr;
    GW2Load_OnLoadLauncher onLoadLauncher = nullptr;
    GW2Load_OnClose onClose = nullptr;
    GW2Load_OnAddonDescriptionVersionOutdated onOutdated = nullptr;
    GW2Load_UpdateCheck updateCheck = nullptr;

    GW2Load_AddonDescription desc;

    std::vector<unsigned char> updateData;
    bool updateDataIsFileName = false;
};

std::vector<AddonData> g_Addons;
bool g_AddonsInitialized = false;

std::vector<AddonData*> g_DelayedAddons;
std::future<void> g_DelayedAddonsManagerFuture;

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
    else if (name == "GW2Load_UpdateCheck")
        data.hasUpdateCheck = true;

    return true;
}

HANDLE g_CurrentProcess;

std::optional<AddonData> InspectAddon(const std::filesystem::path& path)
{
    auto dllBase = SymLoadModuleExW(g_CurrentProcess, nullptr, path.c_str(), nullptr, 0, 0, nullptr, 0);
    if (dllBase == 0)
    {
        spdlog::warn("Could not load module {}: {}", path.string(), GetLastErrorMessage());
        return std::nullopt;
    }

    AddonData data{ path };

    if (!SymEnumSymbols(g_CurrentProcess, dllBase, "GW2Load_*", EnumSymProc, &data))
    {
        spdlog::warn("Could not enumerate symbols for {}: {}", data.file.string(), GetLastErrorMessage());
        return std::nullopt;
    }

    if (!SymUnloadModule(g_CurrentProcess, dllBase))
        spdlog::warn("Could not unload module {}: {}", data.file.string(), GetLastErrorMessage());

    if (data.hasGetAddonDesc)
        return data;
    else
        return std::nullopt;
}

void EnumerateAddons()
{
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &g_CurrentProcess, 0, false, DUPLICATE_SAME_ACCESS))
    {
        spdlog::critical("Could not acquire process handle: {}", GetLastErrorMessage());
        return;
    }

    if (!SymInitialize(g_CurrentProcess, nullptr, false))
    {
        spdlog::critical("Could not initialize symbol handler: {}", GetLastErrorMessage());
        return;
    }

    for (const auto& dir : std::filesystem::directory_iterator{ "addons" })
    {
        if (!dir.is_directory()) continue;
        auto dirName = dir.path().filename().string();
        if (dirName.starts_with(".") || dirName.starts_with("_")) continue;

        for (const auto& file : std::filesystem::directory_iterator{ dir.path() })
        {
            if (!file.is_regular_file()) continue;
            if (!file.path().has_extension() || ToLower(file.path().extension().string()) != ".dll") continue;

            auto data = InspectAddon(file.path());
            if(data)
                g_Addons.push_back(std::move(*data));
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

template<typename F, typename... Args> requires (!std::is_void_v<std::invoke_result_t<F>> && !std::is_same_v<bool, std::invoke_result_t<F>>)
std::optional<std::invoke_result_t<F>> SafeCall(F&& func, auto&& err)
{
    __try
    {
        return func();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        err();
    }

    return std::nullopt;
}

template<typename F, typename... Args> requires std::is_void_v<std::invoke_result_t<F>>
bool SafeCall(F&& func, auto&& err)
{
    __try
    {
        func();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        err();
    }

    return false;
}

template<typename F, typename... Args> requires std::is_same_v<bool, std::invoke_result_t<F>>
bool SafeCall(F&& func, auto&& err)
{
    __try
    {
        return func();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        err();
    }

    return false;
}

AddonData* g_UpdateAddon = nullptr;
void UpdateNotificationCallback(void* data, unsigned int sizeInBytes, bool dataIsFileName)
{
    if (!g_UpdateAddon || sizeInBytes == 0)
        return;

    auto* src = static_cast<const unsigned char*>(data);
    g_UpdateAddon->updateData.assign(src, src + sizeInBytes);
    g_UpdateAddon->updateDataIsFileName = dataIsFileName;
}

bool InitializeAddon(AddonData& addon, bool launcher)
{
    GW2Load_API api{ RegisterCallback };

    auto onError = [&]<typename... Args>(spdlog::format_string_t<Args...> fmt, Args&& ...args) {
        return [&] {
            spdlog::error(std::move(fmt), std::forward<Args>(args)...);
            FreeLibrary(addon.handle);
            addon.handle = nullptr;
            return false;
            };
        };

    if (launcher)
    {
        if (!addon.handle)
        {
            addon.handle = LoadLibrary(addon.file.c_str());
            if (addon.handle == nullptr)
            {
                spdlog::error("Addon {} could not be loaded: {}", addon.file.string(), GetLastErrorMessage());
                return false;
            }

            addon.getAddonDesc = reinterpret_cast<GW2Load_GetAddonDescription>(GetProcAddress(addon.handle, "GW2Load_GetAddonDescription"));
            if (!addon.getAddonDesc)
                return onError("Addon {} does not properly export GetAddonDescription, unloading...", addon.file.string())();

            if (addon.hasOnLoad)
                addon.onLoad = reinterpret_cast<GW2Load_OnLoad>(GetProcAddress(addon.handle, "GW2Load_OnLoad"));
            if (addon.hasOnLoadLauncher)
                addon.onLoadLauncher = reinterpret_cast<GW2Load_OnLoadLauncher>(GetProcAddress(addon.handle, "GW2Load_OnLoadLauncher"));
            if (addon.hasOnClose)
                addon.onClose = reinterpret_cast<GW2Load_OnClose>(GetProcAddress(addon.handle, "GW2Load_OnClose"));
            if (addon.hasOnOutdated)
                addon.onOutdated = reinterpret_cast<GW2Load_OnAddonDescriptionVersionOutdated>(GetProcAddress(addon.handle, "GW2Load_OnAddonDescriptionVersionOutdated"));
            if (addon.hasUpdateCheck)
                addon.updateCheck = reinterpret_cast<GW2Load_UpdateCheck>(GetProcAddress(addon.handle, "GW2Load_UpdateCheck"));

            // Only do the update check on the initial addon load pass, not on subsequent calls to InitializeAddon from the self-update process
            if (addon.updateCheck && !g_AddonsInitialized)
            {
                g_DelayedAddons.emplace_back(&addon);
                return false;
            }
        }

        if (!SafeCall([&] {
            if (!addon.getAddonDesc(&addon.desc))
                return onError("Addon {} refused to load, unloading...", addon.file.string())();
            return true;
            }, onError("Error in addon {} GetAddonDescription, unloading...", addon.file.string())))
        {
            return false;
        }

                switch (addon.desc.descriptionVersion)
                {
                case GW2Load_CurrentAddonDescriptionVersion:
                    break;
                    // Implement backwards compatibility cases here
                default:
                {
                    if (addon.desc.descriptionVersion < GW2Load_CurrentAddonDescriptionVersion)
                    {
                        return onError("Addon {} uses API version {}, which is too old for current loader API version {}, unloading...",
                            addon.file.string(), addon.desc.descriptionVersion, PrintDescVersion(GW2Load_CurrentAddonDescriptionVersion))();
                    }
                    else if (uint32_t addonVer = addon.desc.descriptionVersion; addon.onOutdated)
                    {
                        if (!SafeCall([&] {
                            if (addon.onOutdated(GW2Load_CurrentAddonDescriptionVersion, &addon.desc))
                            {
                                spdlog::warn("Addon {} uses API version {}, which is newer than current loader API version {}; this is okay, as the addon supports backwards compatibility, but consider upgrading your loader.",
                                    addon.file.string(), addonVer, PrintDescVersion(GW2Load_CurrentAddonDescriptionVersion));
                            }
                            }, onError("Error in addon {} OnAddonDescriptionVersionOutdated, unloading...", addon.desc.name)))
                        {
                            return false;
                        }
                    }

                    if (addon.desc.descriptionVersion > GW2Load_CurrentAddonDescriptionVersion)
                    {
                        return onError("Addon {} uses API version {}, which is newer than current loader API version {}, unloading...",
                            addon.file.string(), addon.desc.descriptionVersion, PrintDescVersion(GW2Load_CurrentAddonDescriptionVersion))();
                    }
                }
                }

                spdlog::info("Addon {} recognized as {} v{}.{}.{}",
                    addon.file.string(), addon.desc.name, addon.desc.majorAddonVersion, addon.desc.minorAddonVersion, addon.desc.patchAddonVersion);
    }

    if (launcher && addon.onLoadLauncher)
    {
        g_CallbackAddon = &addon;

        if (!SafeCall(
            [&] {
                return addon.onLoadLauncher(&api);
            },
            onError("Error in addon {} OnLoadLauncher, unloading...", addon.desc.name)))
        {
            return onError("Addon {} OnLoadLauncher signaled a problem, unloading...", addon.desc.name)();
        }

        g_CallbackAddon = nullptr;
        spdlog::debug("Addon {} OnLoadLauncher called.", addon.desc.name);
    }

    if (!launcher && addon.onLoad)
    {
        g_CallbackAddon = &addon;
        if (!SafeCall(
            [&] {
                return addon.onLoad(&api, g_SwapChain, g_Device, g_DeviceContext);
            },
            onError("Error in addon {} OnLoad, unloading...", addon.desc.name)))
        {
            return onError("Addon {} OnLoad signaled a problem, unloading...", addon.desc.name)();
        }
        g_CallbackAddon = nullptr;
        spdlog::debug("Addon {} OnLoad called.", addon.desc.name);
    }

    return true;
}

void UpdateAddon(AddonData* addon)
{
    GW2Load_UpdateAPI api{ UpdateNotificationCallback };
    g_UpdateAddon = addon;
    Cleanup cleanAddon([] { g_UpdateAddon = nullptr; });

    if (SafeCall([&] {
        addon->updateCheck(&api);
        }, [&] {
            spdlog::error("Error in addon {} UpdateCheck, unloading...", addon->file.string());
            FreeLibrary(addon->handle);
            addon->handle = nullptr;
            return false;
            }))
    {
        if (!addon->updateData.empty())
        {
            FreeLibrary(addon->handle);
            addon->handle = nullptr;

            if (!std::filesystem::remove(addon->file))
            {
                spdlog::error("Addon {} could not be removed for updating, aborting...", addon->file.string());
                return;
            }

            if (addon->updateDataIsFileName)
            {
                std::string_view updatedFileName{ reinterpret_cast<const char*>(addon->updateData.data()), addon->updateData.size() };
                auto updatedFile = addon->file.parent_path() / updatedFileName;
                if (!std::filesystem::exists(updatedFile))
                {
                    spdlog::error("Addon {} UpdateCheck provided update file {} which does not exist, aborting...", addon->file.string(), updatedFile.string());
                    return;
                }
                std::filesystem::rename(updatedFile, addon->file);
            }
            else
            {
                FILE* file = nullptr;
                if (fopen_s(&file, addon->file.string().c_str(), "wb") == EINVAL || file == nullptr)
                {
                    spdlog::error("Addon file {} could not be opened for writing, aborting...", addon->file.string());
                    return;
                }
                fwrite(addon->updateData.data(), sizeof(unsigned char), addon->updateData.size(), file);
                fclose(file);
            }

            auto newAddon = InspectAddon(addon->file);
            if (newAddon)
                *addon = *newAddon;
            else
            {
                spdlog::error("Addon {} could not be reloaded after update, aborting...", addon->file.string());
                return;
            }
        }

        InitializeAddon(*addon, true);
    }
}

void InitializeAddons(bool launcher)
{
    for (auto& addon : g_Addons)
        InitializeAddon(addon, launcher);
    g_AddonsInitialized = true;

    for (auto&& [i, cbs] : g_Callbacks)
        std::ranges::sort(cbs, std::greater{}, &PriorityCallback::priority);

    if (!g_DelayedAddons.empty())
    {
        g_DelayedAddonsManagerFuture = std::async(std::launch::async, [] {
            for (auto* addon : g_DelayedAddons)
                UpdateAddon(addon);
            });
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

void LauncherClosing(HWND hwnd)
{
    using namespace std::chrono_literals;
    if (!g_DelayedAddonsManagerFuture.valid() && g_DelayedAddonsManagerFuture.wait_for(2s) != std::future_status::ready)
    {
        // FIXME: Do something
    }

    SymCleanup(g_CurrentProcess);
    CloseHandle(g_CurrentProcess);
}