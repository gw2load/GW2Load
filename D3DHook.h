#pragma once

#include "Common.h"

struct IDXGISwapChain;
struct IDXGISwapChain1;
struct IDXGISwapChain3;

bool InitializeD3DHook();
void InitializeD3DObjects(IDXGISwapChain* swc);
void ShutdownD3DObjects(HWND hWnd);
void OverwriteSwapChainVTables(void* sc);
void OverwriteFactoryVTables(void* dxgiFactory);
void HookDXGIFactories();
void RestoreVTables();
bool AssociatedWithGameWindow(void* sc);

template<typename T>
REFIID GetUUIDOf();

extern struct IDXGISwapChain* g_SwapChain;
extern struct ID3D11Device* g_Device;
extern struct ID3D11DeviceContext* g_DeviceContext;
extern HWND g_AssociatedWindow;

IDXGISwapChain* Downcast(IDXGISwapChain* swc);
IDXGISwapChain* Downcast(IDXGISwapChain1* swc);
IDXGISwapChain* Downcast(IDXGISwapChain2* swc);
IDXGISwapChain* Downcast(IDXGISwapChain3* swc);
IDXGISwapChain* Downcast(IDXGISwapChain4* swc);

IDXGIFactory* Downcast(IDXGIFactory* f);
IDXGIFactory* Downcast(IDXGIFactory1* f);
IDXGIFactory* Downcast(IDXGIFactory2* f);
IDXGIFactory* Downcast(IDXGIFactory3* f);
IDXGIFactory* Downcast(IDXGIFactory4* f);
IDXGIFactory* Downcast(IDXGIFactory5* f);
IDXGIFactory* Downcast(IDXGIFactory6* f);
IDXGIFactory* Downcast(IDXGIFactory7* f);