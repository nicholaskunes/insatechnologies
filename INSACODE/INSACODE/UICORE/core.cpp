#include "../GLOBAL.h"

UICore* UICORE;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

VOID UICore::setError(UINT errorCode)
{
	this->g_lastError = errorCode;
}

LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (UICORE->g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            UICORE->WaitForLastSubmittedFrame();
            ImGui_ImplDX12_InvalidateDeviceObjects();

            UICORE->WaitForLastSubmittedFrame();

            for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
                if (UICORE->g_mainRenderTargetResource[i]) { UICORE->g_mainRenderTargetResource[i]->Release(); UICORE->g_mainRenderTargetResource[i] = NULL; }

            DXGI_SWAP_CHAIN_DESC1 sd;
            UICORE->g_pSwapChain->GetDesc1(&sd);
            sd.Width = (UINT)LOWORD(lParam);
            sd.Height = (UINT)HIWORD(lParam);

            IDXGIFactory4* dxgiFactory = NULL;
            UICORE->g_pSwapChain->GetParent(IID_PPV_ARGS(&dxgiFactory));

            UICORE->g_pSwapChain->Release();
            CloseHandle(UICORE->g_hSwapChainWaitableObject);

            IDXGISwapChain1* swapChain1 = NULL;
            dxgiFactory->CreateSwapChainForHwnd(UICORE->g_pd3dCommandQueue, UICORE->g_hwnd, &sd, NULL, NULL, &swapChain1);
            swapChain1->QueryInterface(IID_PPV_ARGS(&UICORE->g_pSwapChain));
            swapChain1->Release();
            dxgiFactory->Release();

            UICORE->g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);

            UICORE->g_hSwapChainWaitableObject = UICORE->g_pSwapChain->GetFrameLatencyWaitableObject();
            assert(UICORE->g_hSwapChainWaitableObject != NULL);


            for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
            {
                ID3D12Resource* pBackBuffer = NULL;
                UICORE->g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
                UICORE->g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, UICORE->g_mainRenderTargetDescriptor[i]);
                UICORE->g_mainRenderTargetResource[i] = pBackBuffer;
            }

            ImGui_ImplDX12_CreateDeviceObjects();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

VOID UICore::WaitForLastSubmittedFrame()
{
    FrameContext* frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtxt->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

BOOL UICore::CreateRenderD3DDevice()
{

    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
    {
        pdx12Debug->EnableDebugLayer();
        pdx12Debug->Release();
    }
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_fenceEvent == NULL)
        return false;

    {
        IDXGIFactory4* dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1 = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK ||
            dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, this->g_hwnd, &sd, NULL, NULL, &swapChain1) != S_OK ||
            swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    }

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }

    return true;
}

BOOL UICore::DestroyRenderD3DDevice()
{
    FrameContext* frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue == 0)
        return FALSE;

    frameCtxt->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return FALSE;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = NULL; }

    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_hSwapChainWaitableObject != NULL) { CloseHandle(g_hSwapChainWaitableObject); }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = NULL; }
    if (g_pd3dCommandQueue) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = NULL; }
    if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
    if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = NULL; }
    if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }
    if (g_fence) { g_fence->Release(); g_fence = NULL; }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = NULL;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif

    return true;
}

VOID UICore::ShowWindow()
{
    ::ShowWindow(this->g_hwnd, SW_SHOWDEFAULT);
}

VOID UICore::UpdateWindow()
{
    ::UpdateWindow(this->g_hwnd);
}

VOID UICore::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(this->g_hwnd);
    ImGui_ImplDX12_Init(g_pd3dDevice, NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
}

VOID UICore::UIThread(UICore* UICORE)
{
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);


    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {

        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }


        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();


        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);


        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");

            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);

            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }


        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }


        UINT nextFrameIndex = UICORE->g_frameIndex + 1;
        UICORE->g_frameIndex = nextFrameIndex;

        HANDLE waitableObjects[] = { UICORE->g_hSwapChainWaitableObject, NULL };
        DWORD numWaitableObjects = 1;

        FrameContext* frameCtxt = &UICORE->g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
        UINT64 fenceValue = frameCtxt->FenceValue;
        if (fenceValue != 0)
        {
            frameCtxt->FenceValue = 0;
            UICORE->g_fence->SetEventOnCompletion(fenceValue, UICORE->g_fenceEvent);
            waitableObjects[1] = UICORE->g_fenceEvent;
            numWaitableObjects = 2;
        }

        WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);
        UINT backBufferIdx = UICORE->g_pSwapChain->GetCurrentBackBufferIndex();
        frameCtxt->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = UICORE->g_mainRenderTargetResource[backBufferIdx];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        UICORE->g_pd3dCommandList->Reset(frameCtxt->CommandAllocator, NULL);
        UICORE->g_pd3dCommandList->ResourceBarrier(1, &barrier);
        UICORE->g_pd3dCommandList->ClearRenderTargetView(UICORE->g_mainRenderTargetDescriptor[backBufferIdx], (float*)&clear_color, 0, NULL);
        UICORE->g_pd3dCommandList->OMSetRenderTargets(1, &UICORE->g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);
        UICORE->g_pd3dCommandList->SetDescriptorHeaps(1, &UICORE->g_pd3dSrvDescHeap);
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), UICORE->g_pd3dCommandList);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        UICORE->g_pd3dCommandList->ResourceBarrier(1, &barrier);
        UICORE->g_pd3dCommandList->Close();

        UICORE->g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&UICORE->g_pd3dCommandList);

        UICORE->g_pSwapChain->Present(1, 0);


        fenceValue = UICORE->g_fenceLastSignaledValue + 1;
        UICORE->g_pd3dCommandQueue->Signal(UICORE->g_fence, fenceValue);
        UICORE->g_fenceLastSignaledValue = fenceValue;
        frameCtxt->FenceValue = fenceValue;
    }

    UICORE->Shutdown();
}

VOID UICore::Shutdown()
{
    FrameContext* frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue == 0)
        return;

    frameCtxt->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = NULL; }

    ::DestroyWindow(g_hwnd);
    ::UnregisterClassA(g_windowClass.lpszClassName, g_windowClass.hInstance);
}

UICore::UICore()
{
	std::cout << "UICore attempted initialization with no parameters. Aborting..." << std::endl;
	this->g_initialized = FALSE;
	this->setError(UICORE_NO_WINDOW_PARAMETERS);
}

UICore::UICore(const char* windowTitle, UINT x, UINT y, UINT w, UINT h)
{
	std::cout << "UICore initialized, creating " << windowTitle << " at ( " << x << ", " << y << " )" << std::endl;

    this->g_windowClass = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, windowTitle, NULL };
	::RegisterClassExA(&this->g_windowClass);
	this->g_hwnd = ::CreateWindowA(this->g_windowClass.lpszClassName, windowTitle, WS_OVERLAPPEDWINDOW, x, y, w, h, NULL, NULL, this->g_windowClass.hInstance, NULL);

    if (this->g_hwnd) {
        this->g_initialized = TRUE;
        this->setError(UICORE_SUCCESS);
    }
    else {
        this->g_initialized = FALSE;
        this->setError(UICORE_NO_WINDOW);
    }
}

BOOL UICore::isInitialized()
{
	if (this->g_initialized == TRUE) {
		return TRUE;
		this->setError(UICORE_SUCCESS);
	}

	return FALSE;
}

UINT UICore::getLastError()
{
	return this->g_lastError;
}
