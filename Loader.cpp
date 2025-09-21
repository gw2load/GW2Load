#include "Loader.h"
#include "D3DHook.h"
#include "Utils.h"
#include <dbghelp.h>
#include <d3d11_1.h>
#include <fstream>
#include <future>
#include <regex>

bool g_Quit = false;
std::unordered_map<CallbackIndex, CallbackElement> g_Callbacks;

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

    bool isEnabled = false;

    bool hasGetAddonAPIVersion = false;
    bool hasOnLoad = false;
    bool hasOnLoadLauncher = false;
    bool hasOnClose = false;
    bool hasOnOutdated = false;
    bool hasUpdateCheck = false;

    HMODULE handle = nullptr;

    GW2Load_GetAddonAPIVersion_t getAddonAPIVersion = nullptr;
    GW2Load_OnLoad_t onLoad = nullptr;
    GW2Load_OnLoadLauncher_t onLoadLauncher = nullptr;
    GW2Load_OnClose_t onClose = nullptr;
    GW2Load_OnAddonDescriptionVersionOutdated_t onOutdated = nullptr;
    GW2Load_UpdateCheck_t updateCheck = nullptr;

    std::string name;
    unsigned short majorAddonVersion = 0;
    unsigned short minorAddonVersion = 0;
    unsigned short patchAddonVersion = 0;
    unsigned short fixAddonVersion = 0;
    GW2Load_Version_t apiVersion = 0;
    std::string addonVersionString;

    std::chrono::system_clock::time_point updateStartTime;
    std::vector<unsigned char> updateData;
    bool updateDataIsFileName = false;
};

std::vector<AddonData> g_Addons;
struct UpdateData
{
    std::filesystem::path file;
    std::future<void> future;
};
std::vector<UpdateData> g_AddonUpdates;
bool g_AddonsInitialized = false;

BOOL CALLBACK EnumSymProc(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext)
{
    auto& data = *static_cast<AddonData*>(UserContext);

    std::string_view name{ pSymInfo->Name, pSymInfo->NameLen };

    if (name == "GW2Load_GetAddonAPIVersion")
        data.hasGetAddonAPIVersion = true;
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

struct InspectionHandle
{
    HANDLE process = nullptr;
    bool symInitialized = false;
    InspectionHandle()
    {
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &process, 0, false, DUPLICATE_SAME_ACCESS))
        {
            spdlog::critical("Could not acquire process handle: {}", GetLastErrorMessage());
            return;
        }

        if (!SymInitialize(process, nullptr, false))
        {
            spdlog::critical("Could not initialize symbol handler: {}", GetLastErrorMessage());
            return;
        }

        symInitialized = true;
    }

    ~InspectionHandle()
    {
        if (process)
        {
            if (symInitialized)
                SymCleanup(process);

            CloseHandle(process);
        }
    }
};

std::optional<AddonData> InspectAddon(const std::filesystem::path& path, InspectionHandle& inspectHandle)
{
    spdlog::debug("Inspecting potential addon '{}'.", path.string());

    const auto rootPath = std::filesystem::current_path() / path;
    DWORD handle;
    auto fileVersionSize = GetFileVersionInfoSizeW(rootPath.wstring().c_str(), &handle);
    if(fileVersionSize == 0)
    {
        // Don't warn here; this is likely just not a compatible addon (no version info)
        spdlog::debug("Rejected potential addon '{}': no file version info.", path.string());
        return std::nullopt;
    }
    std::vector<unsigned char> fileVersionData(fileVersionSize);
    if (FAILED(GetFileVersionInfoW(rootPath.wstring().c_str(), handle, fileVersionSize, fileVersionData.data())))
    {
        spdlog::warn("Rejected potential addon '{}': could not obtain file version info, error: {}", path.string(), GetLastErrorMessage());
        return std::nullopt;
    }

    const VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoSize;
    if (VerQueryValueW(fileVersionData.data(), L"\\", (void**)(&fileInfo), &fileInfoSize) == 0)
    {
        spdlog::warn("Rejected potential addon '{}': could not obtain fixed file version info.", path.string());
        return std::nullopt;
    }

    const auto [hiVer, loVer] = [&] {
        if (fileInfo->dwProductVersionMS + fileInfo->dwProductVersionLS != 0)
            return std::make_pair(fileInfo->dwProductVersionMS, fileInfo->dwProductVersionLS);
        else
            return std::make_pair(fileInfo->dwFileVersionMS, fileInfo->dwFileVersionLS);
        }();

    AddonData data;
    data.file = path;

    data.majorAddonVersion = HIWORD(hiVer);
    data.minorAddonVersion = LOWORD(hiVer);
    data.patchAddonVersion = HIWORD(loVer);
    data.fixAddonVersion = LOWORD(loVer);

    spdlog::debug("Detected version for potential addon '{}' as {}.{}.{}.{}.", path.string(), data.majorAddonVersion, data.minorAddonVersion, data.patchAddonVersion, data.fixAddonVersion);

    const LANGANDCODEPAGE* lpTranslate = nullptr;
    UINT cbTranslate;
    if (!VerQueryValueW(fileVersionData.data(), L"\\VarFileInfo\\Translation", (void**)&lpTranslate, &cbTranslate))
    {
        spdlog::warn("Rejected potential addon '{}': could not obtain file version info translation.", path.string());
        return std::nullopt;
    }

    char* fileInfoBuf;
    const auto query = std::format("\\StringFileInfo\\{:04x}{:04x}\\ProductName", lpTranslate->wLanguage, lpTranslate->wCodePage);
    if (VerQueryValueA(fileVersionData.data(), query.c_str(), (void**)&fileInfoBuf, &fileInfoSize) == 0 || fileInfoSize == 0)
    {
        const auto query2 = std::format("\\StringFileInfo\\{:04x}{:04x}\\FileDescription", lpTranslate->wLanguage, lpTranslate->wCodePage);
        if (VerQueryValueA(fileVersionData.data(), query2.c_str(), (void**)&fileInfoBuf, &fileInfoSize) == 0 || fileInfoSize == 0)
        {
            spdlog::warn("Rejected potential addon '{}': could not obtain addon name info.", path.string());
            return std::nullopt;
        }
        else
            spdlog::debug("Acquired potential addon '{}' name via FileDescription: {}.", path.string(), fileInfoBuf);
    }
    else
        spdlog::debug("Acquired potential addon '{}' name via ProductName: {}.", path.string(), fileInfoBuf);
    data.name = fileInfoBuf;

    const auto query3 = std::format("\\StringFileInfo\\{:04x}{:04x}\\ProductVersion", lpTranslate->wLanguage, lpTranslate->wCodePage);
    if (VerQueryValueA(fileVersionData.data(), query3.c_str(), (void**)&fileInfoBuf, &fileInfoSize) != 0 && fileInfoSize > 0)
    {
        spdlog::debug("Acquired potential addon '{}' version string via ProductVersion: {}.", path.string(), fileInfoBuf);
        data.addonVersionString = fileInfoBuf;
    }
    else if (const auto query4 = std::format("\\StringFileInfo\\{:04x}{:04x}\\FileVersion", lpTranslate->wLanguage, lpTranslate->wCodePage);
        VerQueryValueA(fileVersionData.data(), query4.c_str(), (void**)&fileInfoBuf, &fileInfoSize) != 0 && fileInfoSize > 0)
    {
        spdlog::debug("Acquired potential addon '{}' version string via FileVersion: {}.", path.string(), fileInfoBuf);
        data.addonVersionString = fileInfoBuf;
    }
    else
    {
        data.addonVersionString = std::format("{}.{}.{}.{}", data.majorAddonVersion, data.minorAddonVersion, data.patchAddonVersion, data.fixAddonVersion);
        spdlog::debug("Derived potential addon '{}' version string from numerical version: {}.", path.string(), data.addonVersionString);
    }

    auto dllBase = SymLoadModuleExW(inspectHandle.process, nullptr, path.c_str(), nullptr, 0, 0, nullptr, SLMFLAG_NO_SYMBOLS);
    if (dllBase == 0)
    {
        spdlog::warn("Rejected potential addon '{}': could not load module for inspection: {}", path.string(), GetLastErrorMessage());
        return std::nullopt;
    }

    if (!SymEnumSymbols(inspectHandle.process, dllBase, "GW2Load_*", EnumSymProc, &data))
    {
        spdlog::warn("Rejected potential addon '{}': could not enumerate symbols: {}", path.string(), GetLastErrorMessage());
        return std::nullopt;
    }

#define PRINT_SYM(sym) if(data.has##sym) spdlog::debug("Potential addon '{}' has export {}.", path.string(), #sym)

    PRINT_SYM(OnLoad);
    PRINT_SYM(OnLoadLauncher);
    PRINT_SYM(OnClose);
    PRINT_SYM(OnOutdated);
    PRINT_SYM(UpdateCheck);

#undef PRINT_SYM

    if (!SymUnloadModule(inspectHandle.process, dllBase))
        spdlog::warn("Could not unload addon '{}' for inspection: {}", path.string(), GetLastErrorMessage());

    if (data.hasGetAddonAPIVersion)
    {
        spdlog::debug("Successfully detected addon '{}'!", path.string());
        return data;
    }
    else
    {
        spdlog::debug("Rejected potential addon '{}': did not find GetAddonAPIVersion export.", path.string());
        return std::nullopt;
    }
}

void EnumerateAddons(const std::filesystem::path& addonsPath, const std::regex& regex)
{
    spdlog::debug("Enumerating addons in '{}'...", addonsPath.string());
    InspectionHandle handle;
    if (!handle.symInitialized)
    {
        spdlog::error("Could not initialize symbol inspection!");
        return;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(addonsPath, ec))
    {
        spdlog::warn("Attempted to enumerate files in path '{}', but it is not a directory.", addonsPath.string());
        return;
    }

    auto recurse = [regex, &handle](this auto const& self, const std::filesystem::path& basePath) -> void {
        for (const auto& entry : std::filesystem::directory_iterator{ basePath, std::filesystem::directory_options::follow_directory_symlink })
        {
            if (entry.is_directory())
            {
                auto dirName = entry.path().filename().string();
                if (dirName.starts_with(".") || dirName.starts_with("_"))
                {
                    spdlog::debug("Skipping directory '{}'.", dirName);
                    continue;
                }

                self(entry.path());
            }
            else
            {
                if (!entry.is_regular_file())
                {
                    spdlog::debug("Skipping file '{}' because it is not a regular file.", entry.path().string());
                    continue;
                }

                if (!entry.path().has_filename())
                {
                    spdlog::debug("Skipping '{}' because it has no file name.", entry.path().string());
                    continue;
                }

                if (!std::regex_match(entry.path().filename().string(), regex))
                {
                    spdlog::debug("Skipping '{}' because it does not match the filter expression.", entry.path().string());
                    continue;
                }

                auto data = InspectAddon(entry.path(), handle);
                if (data) {
                    data->isEnabled = entry.path().has_extension() && entry.path().extension() == ".dll";
                    spdlog::debug("Found addon at path '{}'! It is {}.", entry.path().string(), data->isEnabled ? "enabled" : "disabled");
                    g_Addons.push_back(std::move(*data));
                }
            }
        }
    };

    recurse(addonsPath);
}

std::vector<GW2Load_EnumeratedAddon> g_EnumeratedAddons;
std::vector<std::string> g_EnumeratedAddonStrings;
extern "C" __declspec(dllexport) GW2Load_EnumeratedAddon* GW2Load_GetAddonsInDirectory(const char* directory, unsigned int* count, const char* pattern)
{
    if (!IsAttachedToGame())
    {
        std::regex regex;

        if (!pattern) {
            regex = std::regex(".*\\.dll");
        }
        else {
            try {
                regex = std::regex(pattern);
            }
            catch (std::regex_error&) {
                spdlog::error("Cannot enumerate add-ons. Invalid pattern provided: {}", pattern);
                return nullptr;
            }
        }

        g_Addons.clear();
        EnumerateAddons(directory, regex);
    }

    g_EnumeratedAddons.clear();
    g_EnumeratedAddonStrings.clear();
    g_EnumeratedAddons.reserve(g_Addons.size());
    g_EnumeratedAddonStrings.reserve(g_Addons.size() * 2);

    for (const auto& addon : g_Addons)
    {
        g_EnumeratedAddonStrings.push_back(addon.file.string());
        auto* file = g_EnumeratedAddonStrings.back().c_str();
        g_EnumeratedAddonStrings.push_back(addon.name);
        auto* name = g_EnumeratedAddonStrings.back().c_str();
        g_EnumeratedAddons.emplace_back(file, name, addon.isEnabled);
    }

    *count = static_cast<unsigned int>(g_EnumeratedAddons.size());
    return g_EnumeratedAddons.data();
}

extern "C" __declspec(dllexport) bool GW2Load_CheckIfAddon(const char* path)
{
    std::filesystem::path addonPath(path);
    if (std::filesystem::exists(addonPath))
    {
        InspectionHandle handle;
        return InspectAddon(addonPath, handle).has_value();
    }
    else
        return false;
}

// TODO: use std::optional instead, when it is implemented in C++26
AddonData* GetAddonFromAddress(void* address = _ReturnAddress())
{
    HMODULE mod;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (char*)address, &mod) == false)
    {
        spdlog::warn("GetModuleNameFromAddress: could not retrieve module handle for {}.", fmt::ptr(address));
        return nullptr;
    }

    auto&& addonIt = std::ranges::find(g_Addons, mod, &AddonData::handle);
    if (addonIt != g_Addons.end())
    {
        FreeLibrary(mod);
        return &*addonIt;
    }

    spdlog::warn("GetModuleNameFromAddress: could not find module handle '{}' in addons.", fmt::ptr(mod));
    FreeLibrary(mod);
    return nullptr;
}

std::string_view GetAddonNameFromAddress(void* address = _ReturnAddress())
{
    auto* data = GetAddonFromAddress(address);
    if (data)
    {
        return data->name;
    }

    return "unknown";
}

extern "C" __declspec(dllexport) void GW2Load_RegisterCallback(GW2Load_HookedFunction func, int priority, GW2Load_CallbackPoint callbackPoint, GW2Load_GenericCallback callback)
{
    const auto name = GetAddonNameFromAddress();

    spdlog::debug("Registering callback for {}: func={}, priority={}, callbackPoint={}, callback={}",
        name, func, priority, callbackPoint, fptr(callback));

    if (func == GW2Load_HookedFunction::Undefined || func >= GW2Load_HookedFunction::Count)
    {
        spdlog::error("Error when registering callback for {}: invalid function {}.", name, func);
        return;
    }
    if (callbackPoint == GW2Load_CallbackPoint::Undefined || callbackPoint >= GW2Load_CallbackPoint::Count)
    {
        spdlog::error("Error when registering callback for {} at function {}: invalid callback point {}.", name, func, callbackPoint);
        return;
    }
    if (callback == nullptr)
    {
        spdlog::error("Error when registering callback for {} at function {} point {}: null callback.", name, func, callbackPoint);
        return;
    }

    const auto idx = GetIndex(func, callbackPoint);
    auto& callbacks = g_Callbacks[idx];
    std::lock_guard guard(callbacks.lock);

    // add elements sorted into the vector
    auto it = std::ranges::upper_bound(callbacks.callbacks, priority, std::greater{}, &PriorityCallback::priority);
    callbacks.callbacks.emplace(it, priority, callback);
}

extern "C" __declspec(dllexport) void GW2Load_DeregisterCallback(GW2Load_HookedFunction func, GW2Load_CallbackPoint callbackPoint, GW2Load_GenericCallback callback)
{
    const auto name = GetAddonNameFromAddress();

    spdlog::debug("Deregistering callback for {}: func={}, callbackPoint={}, callback={}",
        name, func, callbackPoint, fptr(callback));

    if (func == GW2Load_HookedFunction::Undefined || func >= GW2Load_HookedFunction::Count)
    {
        spdlog::error("Error when deregistering callback for {}: invalid function {}.", name, func);
        return;
    }
    if (callbackPoint == GW2Load_CallbackPoint::Undefined || callbackPoint >= GW2Load_CallbackPoint::Count)
    {
        spdlog::error("Error when deregistering callback for {} at function {}: invalid callback point {}.", name, func, callbackPoint);
        return;
    }
    if (callback == nullptr)
    {
        spdlog::error("Error when deregistering callback for {} at function {} point {}: null callback.", name, func, callbackPoint);
        return;
    }

    const auto idx = GetIndex(func, callbackPoint);
    auto& callbacks = g_Callbacks[idx];
    std::lock_guard guard(callbacks.lock);

    // remove element from the vector
    const auto& [first, last] = std::ranges::remove(callbacks.callbacks, callback, &PriorityCallback::callback);
    callbacks.callbacks.erase(first, last);
}

extern "C" __declspec(dllexport) void GW2Load_Log(GW2Load_LogLevel level, const char* message, size_t messageSize)
{
    const auto name = GetAddonNameFromAddress();
    std::string_view mess(message, messageSize);
    auto lvl = static_cast<spdlog::level::level_enum>(level);
    g_AddonLogger->log(lvl, "[{}] [{}] {}", name, lvl, mess);
}

template<typename F, typename... Args>
requires (!std::is_void_v<std::invoke_result_t<F>> && !std::is_same_v<bool, std::invoke_result_t<F>>)
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

template<typename F, typename... Args>
requires std::is_void_v<std::invoke_result_t<F>>
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

template<typename F, typename... Args>
requires std::is_same_v<bool, std::invoke_result_t<F>>
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

void UpdateNotificationCallback(void* data, unsigned int sizeInBytes, bool dataIsFileName)
{
    auto* addon = GetAddonFromAddress();
    if (!addon || sizeInBytes == 0) {
        spdlog::trace("UpdateNotificationCallback called - {} .. {}", fmt::ptr(addon), sizeInBytes);
        return;
    }

    spdlog::trace("UpdateNotificationCallback called for addon {} - Size: {} - IsFileName: {}", addon->name, sizeInBytes, dataIsFileName);

    auto* src = static_cast<const unsigned char*>(data);
    addon->updateData.assign(src, src + sizeInBytes);
    addon->updateDataIsFileName = dataIsFileName;
}

bool InitializeAddon(AddonData& addon, bool launcher)
{
    spdlog::debug("Initializing addon {}...", addon.name);

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
                spdlog::error("Addon {} could not be loaded: {}", addon.name, GetLastErrorMessage());
                return false;
            }

            spdlog::debug("Loaded addon {} module '{}'.", addon.name, addon.file.string());

            addon.getAddonAPIVersion = reinterpret_cast<GW2Load_GetAddonAPIVersion_t>(GetProcAddress(addon.handle, "GW2Load_GetAddonAPIVersion"));
            if (!addon.getAddonAPIVersion)
                return onError("Addon {} does not properly export GetAddonAPIVersion, unloading...", addon.name)();

            if (addon.hasOnLoad)
                addon.onLoad = reinterpret_cast<GW2Load_OnLoad_t>(GetProcAddress(addon.handle, "GW2Load_OnLoad"));
            if (addon.hasOnLoadLauncher)
                addon.onLoadLauncher = reinterpret_cast<GW2Load_OnLoadLauncher_t>(GetProcAddress(addon.handle, "GW2Load_OnLoadLauncher"));
            if (addon.hasOnClose)
                addon.onClose = reinterpret_cast<GW2Load_OnClose_t>(GetProcAddress(addon.handle, "GW2Load_OnClose"));
            if (addon.hasOnOutdated)
                addon.onOutdated = reinterpret_cast<GW2Load_OnAddonDescriptionVersionOutdated_t>(GetProcAddress(addon.handle, "GW2Load_OnAddonDescriptionVersionOutdated"));
            if (addon.hasUpdateCheck)
                addon.updateCheck = reinterpret_cast<GW2Load_UpdateCheck_t>(GetProcAddress(addon.handle, "GW2Load_UpdateCheck"));

#define PRINT_SYM(sym, addr) if(addon.has##sym) spdlog::debug("Addon {} export {} has address {}.", addon.name, #sym, reinterpret_cast<void*>(addon.addr))

            PRINT_SYM(GetAddonAPIVersion, getAddonAPIVersion);
            PRINT_SYM(OnLoad, onLoad);
            PRINT_SYM(OnLoadLauncher, onLoadLauncher);
            PRINT_SYM(OnClose, onClose);
            PRINT_SYM(OnOutdated, onOutdated);
            PRINT_SYM(UpdateCheck, updateCheck);

#undef PRINT_SYM

            // Only do the update check on the initial addon load pass, not on subsequent calls to InitializeAddon from the self-update process
            if (addon.updateCheck && !g_AddonsInitialized)
            {
                spdlog::debug("Addon {} has update check, delay init.", addon.name);
                return false;
            }
        }

        if (!SafeCall([&] {
            addon.apiVersion = addon.getAddonAPIVersion();
            if (!addon.apiVersion)
                return onError("Addon {} refused to load, unloading...", addon.name)();
            return true;
            }, onError("Error in addon {} GetAddonDescription, unloading...", addon.name)))
        {
            return false;
        }

        switch (addon.apiVersion)
        {
        case GW2Load_CurrentAddonAPIVersion:
            spdlog::debug("Addon {} uses current API version {}.", addon.name, PrintDescVersion(GW2Load_CurrentAddonAPIVersion));
            break;
            // Implement backwards compatibility cases here
        default:
        {
            if (addon.apiVersion < GW2Load_CurrentAddonAPIVersion)
            {
                return onError("Addon {} uses API version {}, which is too old for current loader API version {}, unloading...",
                    addon.name, PrintDescVersion(addon.apiVersion), PrintDescVersion(GW2Load_CurrentAddonAPIVersion))();
            }
            else if (uint32_t addonVer = addon.apiVersion; addon.onOutdated)
            {
                if (!SafeCall([&] {
                    addon.apiVersion = addon.onOutdated(GW2Load_CurrentAddonAPIVersion);
                    if (addon.apiVersion > GW2Load_AddonAPIVersionMagicFlag && addon.apiVersion <= GW2Load_CurrentAddonAPIVersion)
                    {
                        spdlog::warn("Addon {} uses API version {}, which is newer than current loader API version {}; this is okay, as the addon supports backwards compatibility to API version {}, but consider upgrading your loader.",
                            addon.name, PrintDescVersion(addonVer), PrintDescVersion(GW2Load_CurrentAddonAPIVersion), PrintDescVersion(addon.apiVersion));
                    }
                    }, onError("Error in addon {} OnAddonDescriptionVersionOutdated, unloading...", addon.name)))
                {
                    return false;
                }
            }

            if (addon.apiVersion > GW2Load_CurrentAddonAPIVersion)
            {
                return onError("Addon {} uses API version {}, which is newer than current loader API version {}, unloading... Please update your loader!",
                    addon.name, PrintDescVersion(addon.apiVersion), PrintDescVersion(GW2Load_CurrentAddonAPIVersion))();
            }

            if (addon.apiVersion <= GW2Load_AddonAPIVersionMagicFlag)
            {
                return onError("Addon {} uses an invalid API version {}, unloading...",
                    addon.name, addon.apiVersion)();
            }
        }
        }

        spdlog::info("Addon {} recognized as {} {}!", addon.name, addon.name, addon.addonVersionString);
    }

    if (launcher && addon.onLoadLauncher)
    {
        spdlog::debug("Calling addon {} OnLoadLauncher...", addon.name);

        if (!SafeCall(
            [&] {
                return addon.onLoadLauncher();
            },
            onError("Error in addon {} OnLoadLauncher, unloading...", addon.name)))
        {
            return onError("Addon {} OnLoadLauncher signaled a problem, unloading...", addon.name)();
        }

        spdlog::debug("Addon {} OnLoadLauncher called.", addon.name);
    }

    if (!launcher && addon.onLoad)
    {
        spdlog::debug("Calling addon {} OnLoad...", addon.name);

        if (!SafeCall(
            [&] {
                return addon.onLoad(g_SwapChain, g_Device, g_DeviceContext);
            },
            onError("Error in addon {} OnLoad, unloading...", addon.name)))
        {
            return onError("Addon {} OnLoad signaled a problem, unloading...", addon.name)();
        }

        spdlog::debug("Addon {} OnLoad called.", addon.name);
    }

    return true;
}

void UpdateAddon(AddonData& addon)
{
    GW2Load_UpdateAPI updateApi{ UpdateNotificationCallback };

    addon.updateStartTime = std::chrono::system_clock::now();

    spdlog::info("Checking for updates for addon {}...", addon.file.string());

    Cleanup cleanAddon([&addon] {
        auto currentTime = std::chrono::system_clock::now();
        auto dt = currentTime - addon.updateStartTime;
        spdlog::info("Update check for addon {} complete, took {}.", addon.file.string(),
            static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count()) / 1000.f);
        });

    if (SafeCall([&] {
        addon.updateCheck(&updateApi);
        }, [&] {
            spdlog::error("Error in addon {} UpdateCheck, unloading...", addon.file.string());
            FreeLibrary(addon.handle);
            addon.handle = nullptr;
            return false;
            }))
    {
        if (!addon.updateData.empty())
        {
            FreeLibrary(addon.handle);
            addon.handle = nullptr;

            // return true on delete and false on rename
            auto rename_remove = [](const std::filesystem::path& file)
            {
                bool res;
                try {
                res = std::filesystem::remove(file);
                }catch (std::exception& e)
                {
                    spdlog::error("std::filesystem::remove threw: {}", e.what());
                    throw;
                }
                if (!res)
                {
                    spdlog::error("Addon {} could not be removed for updating, renaming...", file.string());

                    // rename old file into something else, removing can potentially fail
                    auto oldFile = file;
                    oldFile += ".old";
                    std::filesystem::rename(file, oldFile);
                }
                return res;
            };

            bool removed;
            if (addon.updateDataIsFileName)
            {
                std::string_view updatedFileName{ reinterpret_cast<const char*>(addon.updateData.data()), addon.updateData.size() };
                auto updatedFile = addon.file.parent_path() / updatedFileName;
                if (!std::filesystem::exists(updatedFile))
                {
                    spdlog::error("Addon {} UpdateCheck provided update file {} which does not exist, aborting...", addon.file.string(), updatedFile.string());
                    InitializeAddon(addon, true);
                    return;
                }
                removed = rename_remove(addon.file);
                std::filesystem::rename(updatedFile, addon.file);
            }
            else
            {
                removed = rename_remove(addon.file);
                std::ofstream stream(addon.file, std::ios_base::binary | std::ios_base::out);
                if (!stream.is_open())
                {
                    spdlog::error("Addon file {} could not be opened for writing, aborting...", addon.file.string());
                    return;
                }
                stream.write(reinterpret_cast<const char*>(addon.updateData.data()), addon.updateData.size());
                stream.close();
            }
            if (!removed)
            {
                spdlog::warn("Addon {} renamed, load old file ...", addon.file.string());
                addon.file += ".out";
                InitializeAddon(addon, true);
                return;
            }

            InspectionHandle handle;
            if (!handle.symInitialized)
            {
                spdlog::warn("Addon {} could not be loaded: !InspectionHandle::symInitialized", addon.name);
                return;
            }

            auto newAddon = InspectAddon(addon.file, handle);
            if (newAddon)
                addon = std::move(*newAddon);
            else
            {
                spdlog::error("Addon {} could not be reloaded after update, aborting...", addon.file.string());
                return;
            }
        }

        InitializeAddon(addon, true);
    }
}

void InitializeAddons(bool launcher)
{
    spdlog::debug("Initializing addons from {}...", launcher ? "launcher" : "game");

    for (auto& addon : g_Addons)
        InitializeAddon(addon, launcher);
    g_AddonsInitialized = true;

    if (launcher)
    {
        spdlog::debug("Starting update checks...");

        for (auto& addon: g_Addons | std::views::filter([](const auto& val)-> bool {return val.updateCheck;}))
        {
            g_AddonUpdates.emplace_back(addon.file, std::async(std::launch::async, [&addon] {UpdateAddon(addon);}));
        }
    }
}

void ShutdownAddons()
{
    spdlog::debug("Unloading addons...");

    for (auto& addon : g_Addons)
    {
        spdlog::debug("Unloading addon {}...", addon.name);

        if (!addon.handle)
            continue;

        if (addon.onClose)
        {
            spdlog::debug("Calling addon {} OnClose...", addon.name);
            addon.onClose();
        }

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
        EnumerateAddons("addons", std::regex(".*\\.dll"));
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
    if (g_Quit) // Avoid quitting twice
        return;

    g_Quit = true;
    ShutdownAddons();
    ShutdownD3DObjects(hwnd);

    spdlog::shutdown();
}

void LauncherClosing(HWND hwnd)
{
    using namespace std::chrono_literals;

    // TODO: use C++26' std::when_all instead
    auto deadline = std::chrono::steady_clock::now() + 5s;
    std::vector<std::string> failedUpdates;
    for (auto& update : g_AddonUpdates) 
    {
        if (update.future.valid() && update.future.wait_until(deadline) != std::future_status::ready)
        {
            spdlog::info("Could not update addon {}, timed out while waiting for update.", update.file.string());
            failedUpdates.emplace_back(update.file.string());
        }
    }

    if (!failedUpdates.empty())
    {
        spdlog::critical("Could not update all addons within 5 seconds of the launcher closing, terminating game!");

        std::stringstream out;
        out << "One or more addons took too long to update. For safety reasons, the game will now close.";
        out << "\nFailing Addons:";
        for (auto& str : failedUpdates) 
            out << "\n- " << str << "";
        auto outstr = out.str();

        MessageBoxA(g_AssociatedWindow, outstr.c_str(), "Addon Update Failed", MB_OK | MB_ICONEXCLAMATION);

        spdlog::shutdown();
        std::terminate();
    }
}