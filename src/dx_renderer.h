#pragma once

#include "dx_render_primitives.h"

struct dx_renderer
{
	static void initialize(uint32 width, uint32 height);

	static void beginFrame(uint32 width, uint32 height);
	static void dummyRender();


	static dx_cbv_srv_uav_descriptor_heap globalDescriptorHeap;

	static dx_descriptor_handle frameResultSRV;
	static dx_texture frameResult;
	static dx_texture depthBuffer;

	static dx_render_target renderTarget;

	static uint32 renderWidth;
	static uint32 renderHeight;
};

