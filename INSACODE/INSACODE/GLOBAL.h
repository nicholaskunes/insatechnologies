/*
 * IMGUI INCLUDES
 */

#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_impl_dx12.h"

/*
 * LIBRARY INCLUDES
 */

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <thread>


/*
 * INSACODE INCLUDES
 */

#include "UICORE/core.h"
#include "SYSCORE/syscore.h";

extern UICore* UICORE;
extern SystemCore* SYSCORE;


/*
 * MACRO LOGIC
 */

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif