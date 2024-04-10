#pragma once

#include "Common.h"

struct IDXGISwapChain;

bool InitializeD3DHook(HWND hWnd);
void InitializeD3DObjects(IDXGISwapChain* swc);
void ShutdownD3DObjects(HWND hWnd);
void OverwriteVTables(void* sc, void* dev, void* ctx);