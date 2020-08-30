#define UICORE_SUCCESS (0x0)
#define UICORE_NO_WINDOW_PARAMETERS (0x1000)
#define UICORE_NO_WINDOW (0x2000)

struct FrameContext
{
	ID3D12CommandAllocator* CommandAllocator;
	UINT64 FenceValue;
};

int const NUM_FRAMES_IN_FLIGHT = 3;
int const NUM_BACK_BUFFERS = 3;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class UICore {

public:
	FrameContext g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
	UINT g_frameIndex = 0;
	ID3D12Device* g_pd3dDevice = NULL;
	ID3D12DescriptorHeap* g_pd3dRtvDescHeap = NULL;
	ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
	ID3D12CommandQueue* g_pd3dCommandQueue = NULL;
	ID3D12GraphicsCommandList* g_pd3dCommandList = NULL;
	ID3D12Fence* g_fence = NULL;
	HANDLE g_fenceEvent = NULL;
	UINT64 g_fenceLastSignaledValue = 0;
	IDXGISwapChain3* g_pSwapChain = NULL;
	HANDLE g_hSwapChainWaitableObject = NULL;
	ID3D12Resource* g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

	BOOL g_initialized = FALSE;
	UINT g_lastError = UICORE_SUCCESS;
	HWND g_hwnd = NULL;
	WNDCLASSEXA g_windowClass;

	VOID setError(UINT errorCode);

	UICore();
	UICore(const char* windowTitle, UINT x, UINT y, UINT w, UINT h);

	BOOL isInitialized();
	UINT getLastError();
	BOOL CreateRenderD3DDevice();
	BOOL DestroyRenderD3DDevice();
	VOID ShowWindow();
	VOID UpdateWindow();
	VOID InitImGui();
	VOID WaitForLastSubmittedFrame();
	static VOID UIThread(UICore* UICORE);
	VOID Shutdown();

};