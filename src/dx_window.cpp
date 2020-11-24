#include "pch.h"
#include "dx_window.h"
#include "dx_context.h"



static bool checkTearingSupport(dx_factory factory)
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	com<IDXGIFactory5> factory5;
	if (SUCCEEDED(factory.As(&factory5)))
	{
		if (FAILED(factory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&allowTearing, sizeof(allowTearing))))
		{
			allowTearing = FALSE;
		}
	}

	return allowTearing == TRUE;
}

static dx_swapchain createSwapChain(HWND windowHandle,
	dx_factory factory, const dx_command_queue& commandQueue,
	uint32 width, uint32 height, uint32 bufferCount, bool tearingSupported, color_depth colorDepth, bool exclusiveFullscreen)
{
	dx_swapchain dxgiSwapChain;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	if (colorDepth == color_depth_8)
	{
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else
	{
		assert(colorDepth == color_depth_10);
		swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	}
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	com<IDXGISwapChain1> swapChain1;
	checkResult(factory->CreateSwapChainForHwnd(
		commandQueue.commandQueue.Get(),
		windowHandle,
		&swapChainDesc,
		0,
		0,
		&swapChain1));

	UINT flags = 0;
	if (!exclusiveFullscreen)
	{
		// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
		// will be handled manually.
		flags = DXGI_MWA_NO_ALT_ENTER;
	}
	checkResult(factory->MakeWindowAssociation(windowHandle, flags));

	checkResult(swapChain1.As(&dxgiSwapChain));

	return dxgiSwapChain;
}

static int32 computeIntersectionArea(int32 ax1, int32 ay1, int32 ax2, int32 ay2, int32 bx1, int32 by1, int32 bx2, int32 by2)
{
	return max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1));
}

static bool checkForHDRSupport(dx_factory factory, RECT windowRect, color_depth colorDepth)
{
	if (colorDepth == color_depth_8)
	{
		return false;
	}

	com<IDXGIAdapter1> dxgiAdapter;
	checkResult(factory->EnumAdapters1(0, &dxgiAdapter));

	uint32 i = 0;
	com<IDXGIOutput> currentOutput;
	com<IDXGIOutput> bestOutput;
	int32 bestIntersectArea = -1;

	while (dxgiAdapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
	{
		// Get the retangle bounds of the app window.
		int ax1 = windowRect.left;
		int ay1 = windowRect.top;
		int ax2 = windowRect.right;
		int ay2 = windowRect.bottom;

		// Get the rectangle bounds of current output.
		DXGI_OUTPUT_DESC desc;
		checkResult(currentOutput->GetDesc(&desc));
		RECT r = desc.DesktopCoordinates;
		int bx1 = r.left;
		int by1 = r.top;
		int bx2 = r.right;
		int by2 = r.bottom;

		// Compute the intersection.
		int32 intersectArea = computeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
		if (intersectArea > bestIntersectArea)
		{
			bestOutput = currentOutput;
			bestIntersectArea = intersectArea;
		}

		++i;
	}

	com<IDXGIOutput6> output6;
	checkResult(bestOutput.As(&output6));

	DXGI_OUTPUT_DESC1 desc1;
	checkResult(output6->GetDesc1(&desc1));

	return desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
}

static void setSwapChainColorSpace(dx_swapchain swapchain, color_depth colorDepth, bool hdrSupport)
{
	// Rec2020 is the standard for UHD displays. The tonemap shader needs to apply the ST2084 curve before display.
	// Rec709 is the same as sRGB, just without the gamma curve. The tonemap shader needs to apply the gamma curve before display.
	DXGI_COLOR_SPACE_TYPE colorSpace = (hdrSupport && colorDepth == color_depth_10) ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	UINT colorSpaceSupport = 0;
	if (SUCCEEDED(swapchain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) &&
		((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
	{
		checkResult(swapchain->SetColorSpace1(colorSpace));
	}

	if (!hdrSupport)
	{
		checkResult(swapchain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, 0));
		return;
	}

	struct display_chromaticities
	{
		float redX;
		float redY;
		float greenX;
		float greenY;
		float blueX;
		float blueY;
		float whiteX;
		float whiteY;
	};

	static const display_chromaticities chroma =
	{
		0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f // Display Gamut Rec2020
	};

	float maxOutputNits = 1000.f;
	float minOutputNits = 0.001f;
	float maxCLL = 2000.f;
	float maxFALL = 500.f;

	DXGI_HDR_METADATA_HDR10 hdr10MetaData = {};
	hdr10MetaData.RedPrimary[0] = (uint16)(chroma.redX * 50000.f);
	hdr10MetaData.RedPrimary[1] = (uint16)(chroma.redY * 50000.f);
	hdr10MetaData.GreenPrimary[0] = (uint16)(chroma.greenX * 50000.f);
	hdr10MetaData.GreenPrimary[1] = (uint16)(chroma.greenY * 50000.f);
	hdr10MetaData.BluePrimary[0] = (uint16)(chroma.blueX * 50000.f);
	hdr10MetaData.BluePrimary[1] = (uint16)(chroma.blueY * 50000.f);
	hdr10MetaData.WhitePoint[0] = (uint16)(chroma.whiteX * 50000.f);
	hdr10MetaData.WhitePoint[1] = (uint16)(chroma.whiteY * 50000.f);
	hdr10MetaData.MaxMasteringLuminance = (uint32)(maxOutputNits * 10000.f);
	hdr10MetaData.MinMasteringLuminance = (uint32)(minOutputNits * 10000.f);
	hdr10MetaData.MaxContentLightLevel = (uint16)(maxCLL);
	hdr10MetaData.MaxFrameAverageLightLevel = (uint16)(maxFALL);

	checkResult(swapchain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10MetaData));
}

static void updateRenderTargetViews(dx_window& window, dx_device device)
{
	uint32 rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(window.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		dx_resource backBuffer;
		checkResult(window.swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), 0, rtvHandle);

		window.backBuffers[i] = backBuffer;

		SET_NAME(window.backBuffers[i], "Backbuffer");

		rtvHandle.Offset(rtvDescriptorSize);
	}
}

bool dx_window::initialize(const TCHAR* name, uint32 clientWidth, uint32 clientHeight, color_depth colorDepth, DXGI_FORMAT depthFormat, bool exclusiveFullscreen)
{
	bool result = win32_window::initialize(name, clientWidth, clientHeight);
	if (!result)
	{
		return false;
	}

	this->colorDepth = colorDepth;
	this->exclusiveFullscreen = exclusiveFullscreen;
	this->depthFormat = depthFormat;
	tearingSupported = checkTearingSupport(dxContext.factory);


	swapchain = createSwapChain(windowHandle, dxContext.factory, dxContext.renderQueue, clientWidth, clientHeight, NUM_BUFFERED_FRAMES, tearingSupported, colorDepth, exclusiveFullscreen);
	currentBackbufferIndex = swapchain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.NumDescriptors = NUM_BUFFERED_FRAMES;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	checkResult(dxContext.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap)));
	rtvDescriptorSize = dxContext.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	RECT windowRect = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
	GetWindowRect(windowHandle, &windowRect);
	hdrSupport = checkForHDRSupport(dxContext.factory, windowRect, colorDepth);
	setSwapChainColorSpace(swapchain, colorDepth, hdrSupport);

	updateRenderTargetViews(*this, dxContext.device);

	assert(depthFormat == DXGI_FORMAT_UNKNOWN || isDepthFormat(depthFormat));
	if (depthFormat != DXGI_FORMAT_UNKNOWN)
	{
		depthBuffer = createDepthTexture(clientWidth, clientHeight, depthFormat);
	}

	initialized = true;

	return true;
}

void dx_window::shutdown()
{
	initialized = false;
	win32_window::shutdown();
}

void dx_window::onResize()
{
	if (initialized)
	{
		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		dxContext.flushApplication();

		for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
		{
			backBuffers[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		checkResult(swapchain->GetDesc(&swapChainDesc));
		checkResult(swapchain->ResizeBuffers(NUM_BUFFERED_FRAMES, clientWidth, clientHeight,
			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		RECT windowRect;
		GetWindowRect(windowHandle, &windowRect);
		hdrSupport = checkForHDRSupport(dxContext.factory, windowRect, colorDepth);
		setSwapChainColorSpace(swapchain, colorDepth, hdrSupport);

		currentBackbufferIndex = swapchain->GetCurrentBackBufferIndex();

		updateRenderTargetViews(*this, dxContext.device);

		if (depthBuffer->resource)
		{
			resizeTexture(depthBuffer, clientWidth, clientHeight);
		}
	}
}

void dx_window::onMove()
{
	if (initialized)
	{
		RECT windowRect;
		GetWindowRect(windowHandle, &windowRect);
		hdrSupport = checkForHDRSupport(dxContext.factory, windowRect, colorDepth);
		setSwapChainColorSpace(swapchain, colorDepth, hdrSupport);
	}
}

void dx_window::onWindowDisplayChange()
{
	if (initialized)
	{
		RECT windowRect;
		GetWindowRect(windowHandle, &windowRect);
		hdrSupport = checkForHDRSupport(dxContext.factory, windowRect, colorDepth);
		setSwapChainColorSpace(swapchain, colorDepth, hdrSupport);
	}
}

void dx_window::swapBuffers()
{
	if (initialized)
	{
		uint32 syncInterval = vSync ? 1 : 0;
		uint32 presentFlags = tearingSupported && !vSync && !exclusiveFullscreen ? DXGI_PRESENT_ALLOW_TEARING : 0;
		checkResult(swapchain->Present(syncInterval, presentFlags));

		currentBackbufferIndex = swapchain->GetCurrentBackBufferIndex();
	}
}

void dx_window::toggleFullscreen()
{
	if (exclusiveFullscreen)
	{
		fullscreen = !fullscreen;
		swapchain->SetFullscreenState(fullscreen, 0);
	}
	else
	{
		win32_window::toggleFullscreen();
	}
}

void dx_window::toggleVSync()
{
	vSync = !vSync;
}
