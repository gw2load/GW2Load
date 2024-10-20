#pragma once

#include "Common.h"

struct IDXGISwapChain;
struct IDXGISwapChain1;
struct IDXGISwapChain3;

struct ID3D11Device;
struct ID3D11DeviceContext;

struct IDXGIFactory;

IDXGIFactory* GetFactoryPointer(REFIID riid, void* factory);

void InitializeSwapChain(IDXGISwapChain* sc);
bool InitializeD3DHook();
void InitializeD3DObjects(IDXGISwapChain* swc);
void ShutdownD3DObjects(HWND hWnd);
void OverwriteDXGIFactoryVTables(IDXGIFactory* factory);
void OverwriteSwapChainVTables(IDXGISwapChain* swc);
std::pair<ID3D11Device*, ID3D11DeviceContext*> GetDeviceFromSwapChain(IDXGISwapChain* sc);
void RestoreVTables();

extern struct IDXGISwapChain* g_SwapChain;
extern struct ID3D11Device* g_Device;
extern struct ID3D11DeviceContext* g_DeviceContext;
extern HWND g_AssociatedWindow;

#define QUERY_VERSIONED_INTERFACE(Name_, Prefix_, Version_) \
Prefix_##Name_##Version_* Get##Name_##Version_(Prefix_##Name_* obj); \
Prefix_##Name_* Downcast(Prefix_##Name_##Version_* obj);

QUERY_VERSIONED_INTERFACE(SwapChain, IDXGI, 1);
QUERY_VERSIONED_INTERFACE(SwapChain, IDXGI, 3);

QUERY_VERSIONED_INTERFACE(Factory, IDXGI, 1);
QUERY_VERSIONED_INTERFACE(Factory, IDXGI, 2);

#undef QUERY_VERSIONED_INTERFACE