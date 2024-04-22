#pragma once

#include "Common.h"

struct IDXGISwapChain;
struct IDXGISwapChain1;
struct IDXGISwapChain3;

bool InitializeD3DHook(HWND hWnd);
void InitializeD3DObjects(IDXGISwapChain* swc);
void ShutdownD3DObjects(HWND hWnd);
void OverwriteVTables(void* sc, void* dev, void* ctx);
IDXGISwapChain1* GetSwapChain1(IDXGISwapChain* sc);
IDXGISwapChain3* GetSwapChain3(IDXGISwapChain* sc);

extern struct IDXGISwapChain* g_SwapChain;
extern struct ID3D11Device* g_Device;
extern struct ID3D11DeviceContext* g_DeviceContext;
extern HWND g_AssociatedWindow;