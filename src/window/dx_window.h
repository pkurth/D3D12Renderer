#pragma once

#include "dx/dx.h"
#include "dx/dx_descriptor.h"
#include "rendering/render_utils.h"
#include "window.h"

struct dx_window : win32_window
{
	dx_window() = default;
	dx_window(dx_window&) = delete;
	dx_window(dx_window&&) = default;

	virtual ~dx_window();

	bool initialize(const TCHAR* name, uint32 requestedClientWidth, uint32 requestedClientHeight, color_depth colorDepth = color_depth_8, bool exclusiveFullscreen = false);

	virtual void shutdown();

	virtual void swapBuffers();
	virtual void toggleFullscreen();
	void toggleVSync();

	virtual void onResize();
	virtual void onMove();
	virtual void onWindowDisplayChange();


	dx_resource backBuffers[NUM_BUFFERED_FRAMES];
	dx_rtv_descriptor_handle backBufferRTVs[NUM_BUFFERED_FRAMES];
	uint32 currentBackbufferIndex;

	color_depth colorDepth;

private:
	void updateRenderTargetViews();

	dx_swapchain swapchain;
	com<ID3D12DescriptorHeap> rtvDescriptorHeap;


	bool tearingSupported;
	bool exclusiveFullscreen;
	bool hdrSupport;
	bool vSync = false;
	bool initialized = false;
};
