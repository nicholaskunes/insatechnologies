#include "GLOBAL.h"

int main(int, char**)
{

    UICORE = new UICore("INSA Code v2020.01a - INSA Technologies", 100, 100, 600, 600);

    if (!UICORE->isInitialized()) {
        std::cout << "UICore initialization failure with code 0x" << std::hex << UICORE->getLastError() << std::dec << std::endl;
    }

    if (!UICORE->CreateRenderD3DDevice()) {
        std::cout << "UICore Create D3D Device and Render Target failure with code 0x" << std::hex << UICORE->getLastError() << std::dec << std::endl;
        UICORE->DestroyRenderD3DDevice();
        ::UnregisterClassA(UICORE->g_windowClass.lpszClassName, UICORE->g_windowClass.hInstance);
    }
   
    UICORE->ShowWindow();
    UICORE->UpdateWindow();
    UICORE->InitImGui(); 

    UICore::UIThread(UICORE);


    return 0;
}

