#pragma once

// GW2Load API Overview

/*
*
* 1. Folder structure
*
* Throughout this document, "." will refer to Guild Wars 2's root installation path.
* GW2Load will look for addons in subfolders of "./addons". DLLs found directly in the addons folder will be ignored.
* Subfolders starting with "." or "_" characters will also be skipped.
* Inside the subfolders, all DLLs will be checked for GW2Load exports. This is done *without* executing any code in the DLL.
*
* 2. Exports
*
* Only one export is required to be recognized as an addon:
* bool GW2Load_GetAddonDescription(GW2Load_AddonDescription* desc);
* The return value will be checked and addon loading will be aborted if it is false.
*
* Additional exports will be detected at this time as well:
* bool GW2Load_OnLoad(GW2Load_API* api, IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context);
* bool GW2Load_OnLoadLauncher(GW2Load_API* api);
* The return value for either of these will be checked and the addon will be unloaded if it is false.
* void GW2Load_OnClose();
*
* For advanced use only:
* bool GW2Load_OnAddonDescriptionVersionOutdated(unsigned int loaderVersion, GW2Load_AddonDescription* desc);
* This will only be called if GW2Load's addon description version is *older* than the addon's, allowing the addon to adjust its behavior for the outdated loader.
* The loader will handle backwards compatibility automatically (e.g., an addon using version 1 being loaded by a loader with version 2).
*
* using GW2Load_UpdateCallback = void(*)(void* data, unsigned int sizeInBytes, bool dataIsFileName);
* void GW2Load_UpdateCheck(GW2Load_UpdateAPI* api);
* If the export is defined, UpdateCheck will be called *before* GetAddonDescription to alllow the addon the opportunity to self-update.
* The provided callback may be called by the addon to signal to the loader that an update is pending.
* The buffer provided by the addon will be copied by the loader so the addon can free the buffer immediately after the callback returns.
* Two interpretations of the data buffer are available:
*   - If dataIsfileName is true, then data is cast as a C-string and interpreted as a path relative to the current DLL's location.
*     The addon will be unloaded and replaced with the new file, then reloaded.
*   - If dataIsfileName is false, then data is assumed to contain the new DLL's binary data in full.
*     The addon will be unloaded and overwritten by the new data, then reloaded.
* These calls are asynchronous: if UpdateCheck exists, a thread will be spawned to perform the update check for each addon in parallel.
* All UpdateCheck threads will be killed at most two seconds after the launcher is closed and the affected addons will be unloaded.
*
* Once API capabilities have been determined, the DLL will be unloaded.
* After all DLLs have been processed, they will be reloaded properly (via LoadLibrary) at the earliest opportunity.
* This is also when GetAddonDescription and UpdateCheck (if defined) will be invoked.
*
* As per Microsoft guidelines, do *NOT* run complex initialization in DllMain. Use one of the OnLoad events instead.
* Similarly, it is recommended to run cleanup operations in the OnClose event.
*
*/

inline static constexpr unsigned int GW2Load_AddonDescriptionVersionMagicFlag = 0xF0CF0000;
inline static constexpr unsigned int GW2Load_CurrentAddonDescriptionVersion = GW2Load_AddonDescriptionVersionMagicFlag | 1;

struct GW2Load_AddonDescription
{
    unsigned int descriptionVersion; // Always set to GW2Load_CurrentAddonDescriptionVersion
    unsigned int majorAddonVersion;
    unsigned int minorAddonVersion;
    unsigned int patchAddonVersion;
    const char* name;
};

enum class GW2Load_HookedFunction : unsigned int
{
    Undefined = 0, // Reserved, do not use
    Present, // void Present(IDXGISwapChain* swapChain);
    ResizeBuffers, // void ResizeBuffers(IDXGISwapChain* swapChain, unsigned int width, unsigned int height, DXGI_FORMAT format);

    Count
};

enum class GW2Load_CallbackPoint : unsigned int
{
    Undefined = 0, // Reserved, do not use
    BeforeCall,
    AfterCall,

    Count
};

using GW2Load_GenericCallback = void(__cdecl*)(); // Used as a placeholder for the actual function signature, see enum
using GW2Load_PresentCallback = void(__cdecl*)(IDXGISwapChain* swapChain);
using GW2Load_ResizeBuffersCallback = void(__cdecl*)(IDXGISwapChain* swapChain, unsigned int width, unsigned int height, DXGI_FORMAT format);

using GW2Load_RegisterCallback = void(__cdecl*)(GW2Load_HookedFunction func, int priority, GW2Load_CallbackPoint callbackPoint, GW2Load_GenericCallback callback);
using GW2Load_UpdateCallback = void(__cdecl*)(void* data, unsigned int sizeInBytes, bool dataIsFileName);

struct GW2Load_API
{
#ifdef __cplusplus
    template<typename F>
    auto RegisterCallback(GW2Load_HookedFunction func, int priority, GW2Load_CallbackPoint callbackPoint, F callback)
    {
        GW2Load_GenericCallback cb = reinterpret_cast<GW2Load_GenericCallback>(+callback);
        registerCallback(func, priority, callbackPoint, cb);
    }
protected:
#endif
    GW2Load_RegisterCallback registerCallback;
};

struct GW2Load_UpdateAPI
{
    GW2Load_UpdateCallback updateCallback;
};

using GW2Load_GetAddonDescription_t = bool(*)(GW2Load_AddonDescription* desc);
using GW2Load_OnLoad_t = bool(*)(GW2Load_API* api, struct IDXGISwapChain* swapChain, struct ID3D11Device* device, struct ID3D11DeviceContext* context);
using GW2Load_OnLoadLauncher_t = bool(*)(GW2Load_API* api);
using GW2Load_OnClose_t = void(*)();
using GW2Load_OnAddonDescriptionVersionOutdated_t = bool(*)(unsigned int loaderVersion, GW2Load_AddonDescription* desc);
using GW2Load_UpdateCheck_t = void(*)(GW2Load_UpdateAPI* api);