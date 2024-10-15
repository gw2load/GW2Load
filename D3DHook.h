#pragma once

#include "Common.h"

struct IDXGISwapChain;
struct IDXGISwapChain1;
struct IDXGISwapChain3;

bool InitializeD3DHook();
void InitializeD3DObjects(IDXGISwapChain* swc);
void ShutdownD3DObjects(HWND hWnd);
void OverwriteDXGIFactoryVTables(void* factory);
void OverwriteVTables(void* sc, void* dev, void* ctx);
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

QUERY_VERSIONED_INTERFACE(Factory, IDXGI, 2);

#undef QUERY_VERSIONED_INTERFACE