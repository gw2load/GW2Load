#define CINTERFACE
#define D3D11_NO_HELPERS

#include "D3DHook.h"
#include <variant>
#include "Utils.h"

namespace {

std::unordered_map<void*, std::function<void()>> g_OverwrittenVTableEntries;

template<typename T, typename F>
concept HookType = std::same_as<std::remove_cvref_t<F>, std::remove_cvref_t<std::add_pointer_t<decltype(T::Hook)>>>
&& std::same_as<std::remove_cvref_t<F>, std::remove_cvref_t<decltype(T::Real)>>;


template<typename T, typename F> requires HookType<T, F>
void HookFunction(F& function) {
	void* functionPtr = static_cast<void*>(&function);

	if(g_OverwrittenVTableEntries.contains(functionPtr))
		return;

	DWORD oldProtect;
	VirtualProtect(functionPtr, sizeof(void*), PAGE_READWRITE, &oldProtect);
	T::Real = function;
	function = T::Hook;
	VirtualProtect(functionPtr, sizeof(void*), oldProtect, &oldProtect);

	g_OverwrittenVTableEntries[functionPtr] = [&function, functionPtr] {
		DWORD oldProtect;
		VirtualProtect(functionPtr, sizeof(void*), PAGE_READWRITE, &oldProtect);
		function = T::Real;
		VirtualProtect(functionPtr, sizeof(void*), oldProtect, &oldProtect);
	};
}

}

template<typename T>
struct HkSwapChainPresent {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::Present) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		UINT SyncInterval,
		UINT Flags) {
		InitializeD3DObjects(Downcast(This));
		InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::BeforeCall, GW2Load_PresentCallback>(Downcast(This));
		auto returnValue = Real(This, SyncInterval, Flags);
		InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::AfterCall, GW2Load_PresentCallback>(Downcast(This));
		return returnValue;
	}
};

template<typename T>
struct HkSwapChainPresent1 {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::Present1) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		UINT SyncInterval,
		UINT PresentFlags,
		const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
		InitializeD3DObjects(Downcast(This));
		InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::BeforeCall, GW2Load_PresentCallback>(Downcast(This));
		auto returnValue = Real(This, SyncInterval, PresentFlags, pPresentParameters);
		InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::AfterCall, GW2Load_PresentCallback>(Downcast(This));
		return returnValue;
	}
};

template<typename T>
struct HkSwapChainResizeBuffers {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::ResizeBuffers) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		UINT BufferCount,
		UINT Width,
		UINT Height,
		DXGI_FORMAT NewFormat,
		UINT SwapChainFlags) {
		InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::BeforeCall, GW2Load_ResizeBuffersCallback>(Downcast(This), Width, Height, NewFormat);
		auto returnValue = Real(This, BufferCount, Width, Height, NewFormat, SwapChainFlags);
		InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::AfterCall, GW2Load_ResizeBuffersCallback>(Downcast(This), Width, Height, NewFormat);
		return returnValue;
	}
};

template<typename T>
struct HkSwapChainResizeBuffers1 {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::ResizeBuffers1) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		UINT BufferCount,
		UINT Width,
		UINT Height,
		DXGI_FORMAT Format,
		UINT SwapChainFlags,
		const UINT* pCreationNodeMask,
		IUnknown* const* ppPresentQueue) {
		InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::BeforeCall, GW2Load_ResizeBuffersCallback>(Downcast(This), Width, Height, Format);
		auto returnValue = Real(This, BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
		InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::AfterCall, GW2Load_ResizeBuffersCallback>(Downcast(This), Width, Height, Format);
		return returnValue;
	}
};


void OverwriteSwapChainVTables(void* baseSC_) {
	spdlog::debug("Attempting to overwrite SwapChain vtables...");
	IDXGISwapChain* baseSC = static_cast<IDXGISwapChain*>(baseSC_);
	auto* baseVT = baseSC->lpVtbl;

	auto forEachVT = [&]<typename T>() {
		constexpr bool NeedsQuerying = !std::same_as<T, IDXGISwapChain>;
		T* sc = NeedsQuerying ? nullptr : reinterpret_cast<T*>(baseSC);
		if(!NeedsQuerying || SUCCEEDED(baseVT->QueryInterface(baseSC, GetUUIDOf<T>(), reinterpret_cast<void**>(&sc)))) {
			auto* vt = sc->lpVtbl;

			HookFunction<HkSwapChainPresent<T>>(vt->Present);
			HookFunction<HkSwapChainResizeBuffers<T>>(vt->ResizeBuffers);

			if constexpr(requires() { vt->Present1; })
				HookFunction<HkSwapChainPresent1<T>>(vt->Present1);
			if constexpr(requires() { vt->ResizeBuffers1; })
				HookFunction<HkSwapChainResizeBuffers1<T>>(vt->ResizeBuffers1);

			// Don't release object being returned by the factory!
			if(NeedsQuerying)
				vt->Release(sc);
		}
	};

	forEachVT.operator()<IDXGISwapChain>();
	forEachVT.operator()<IDXGISwapChain1>();
	forEachVT.operator()<IDXGISwapChain2>();
	forEachVT.operator()<IDXGISwapChain3>();
	forEachVT.operator()<IDXGISwapChain4>();
}

template<typename T>
struct HkFactoryCreateSwapChain {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::CreateSwapChain) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		IUnknown* pDevice,
		DXGI_SWAP_CHAIN_DESC* pDesc,
		IDXGISwapChain** ppSwapChain) {
		auto returnValue = Real(This, pDevice, pDesc, ppSwapChain);
		if(SUCCEEDED(returnValue))
			OverwriteSwapChainVTables(*ppSwapChain);
		return returnValue;
	}
};

template<typename T>
struct HkFactoryCreateSwapChainForComposition {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::CreateSwapChainForComposition) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		IUnknown* pDevice,
		const DXGI_SWAP_CHAIN_DESC1* pDesc,
		IDXGIOutput* pRestrictToOutput,
		IDXGISwapChain1** ppSwapChain) {
		auto returnValue = Real(This, pDevice, pDesc, pRestrictToOutput, ppSwapChain);
		if(SUCCEEDED(returnValue))
			OverwriteSwapChainVTables(*ppSwapChain);
		return returnValue;
	}
};

template<typename T>
struct HkFactoryCreateSwapChainForCoreWindow {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::CreateSwapChainForCoreWindow) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		IUnknown* pDevice,
		IUnknown* pWindow,
		const DXGI_SWAP_CHAIN_DESC1* pDesc,
		IDXGIOutput* pRestrictToOutput,
		IDXGISwapChain1** ppSwapChain) {
		auto returnValue = Real(This, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
		if(SUCCEEDED(returnValue))
			OverwriteSwapChainVTables(*ppSwapChain);
		return returnValue;
	}
};

template<typename T>
struct HkFactoryCreateSwapChainForHwnd {
	using VTable = std::remove_pointer_t<std::remove_cvref_t<decltype(T::lpVtbl)>>;
	inline static decltype(VTable::CreateSwapChainForHwnd) Real = nullptr;
	static HRESULT STDMETHODCALLTYPE Hook(
		T* This,
		IUnknown* pDevice,
		HWND hWnd,
		const DXGI_SWAP_CHAIN_DESC1* pDesc,
		const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
		IDXGIOutput* pRestrictToOutput,
		IDXGISwapChain1** ppSwapChain) {
		auto returnValue = Real(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
		if(SUCCEEDED(returnValue))
			OverwriteSwapChainVTables(*ppSwapChain);
		return returnValue;
	}
};

void OverwriteFactoryVTables(void* baseF_) {
	spdlog::debug("Attempting to overwrite DXGIFactory vtables...");
	IDXGIFactory* baseF = static_cast<IDXGIFactory*>(baseF_);
	auto* baseVT = baseF->lpVtbl;

	auto forEachVT = [&]<typename T>() {
		constexpr bool NeedsQuerying = !std::same_as<T, IDXGIFactory>;
		T* f = NeedsQuerying ? nullptr : reinterpret_cast<T*>(baseF);
		if(!NeedsQuerying || SUCCEEDED(baseVT->QueryInterface(baseF, GetUUIDOf<T>(), reinterpret_cast<void**>(&f)))) {
			auto* vt = f->lpVtbl;

			spdlog::debug("DXGIFactory vtable is new: hooking!");
			HookFunction<HkFactoryCreateSwapChain<T>>(vt->CreateSwapChain);

			if constexpr(requires() { vt->CreateSwapChainForComposition; })
				HookFunction<HkFactoryCreateSwapChainForComposition<T>>(vt->CreateSwapChainForComposition);
			if constexpr(requires() { vt->CreateSwapChainForCoreWindow; })
				HookFunction<HkFactoryCreateSwapChainForCoreWindow<T>>(vt->CreateSwapChainForCoreWindow);
			if constexpr(requires() { vt->CreateSwapChainForHwnd; })
				HookFunction<HkFactoryCreateSwapChainForHwnd<T>>(vt->CreateSwapChainForHwnd);

			// Don't release object being returned by the factory!
			if(NeedsQuerying)
				vt->Release(f);
		}
	};

	forEachVT.operator()<IDXGIFactory>();
	forEachVT.operator()<IDXGIFactory1>();
	forEachVT.operator()<IDXGIFactory2>();
	forEachVT.operator()<IDXGIFactory3>();
	forEachVT.operator()<IDXGIFactory4>();
	forEachVT.operator()<IDXGIFactory5>();
	forEachVT.operator()<IDXGIFactory6>();
	forEachVT.operator()<IDXGIFactory7>();
}

void RestoreVTables()
{
	spdlog::debug("Restoring all vtables...");

	for (auto& u : g_OverwrittenVTableEntries | std::views::values)
	{
		u();
	}
	g_OverwrittenVTableEntries.clear();
}