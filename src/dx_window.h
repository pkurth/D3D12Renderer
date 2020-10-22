#pragma once

#include "dx.h"
#include "window.h"
#include "dx_render_primitives.h"

struct dx_window : win32_window
{
	dx_window() = default;
	dx_window(dx_window&) = delete;
	dx_window(dx_window&&) = default;

	bool initialize(const TCHAR* name, uint32 initialWidth, uint32 initialHeight, color_depth colorDepth, DXGI_FORMAT depthFormat, bool exclusiveFullscreen = false);

	virtual void shutdown();

	virtual void swapBuffers();
	virtual void toggleFullscreen();

	virtual void onResize();
	virtual void onMove();
	virtual void onWindowDisplayChange();



	dx_swapchain swapchain;
	dx_resource backBuffers[NUM_BUFFERED_FRAMES];
	com<ID3D12DescriptorHeap> rtvDescriptorHeap;
	uint32 rtvDescriptorSize;
	uint32 currentBackbufferIndex;

	color_depth colorDepth;

	RECT windowRectBeforeFullscreen;

	bool tearingSupported;
	bool exclusiveFullscreen;
	bool hdrSupport;
	bool fullscreen = false;
	bool vSync = false;
	bool open = true;
	bool initialized = false;

	dx_texture depthBuffer;
	DXGI_FORMAT depthFormat;
};
