#pragma once

#include "dx_render_primitives.h"

struct dx_renderer
{
	static void initialize(uint32 width, uint32 height);

	static void beginFrame(uint32 width, uint32 height, dx_resource screenBackbuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV);



	static dx_texture depthBuffer;
	static uint32 renderWidth;
	static uint32 renderHeight;

	static dx_resource screenBackbuffer;
	static CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV;
};

