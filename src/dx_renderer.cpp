#include "pch.h"
#include "dx_renderer.h"

dx_texture dx_renderer::depthBuffer;
uint32 dx_renderer::renderWidth;
uint32 dx_renderer::renderHeight;

dx_resource dx_renderer::screenBackbuffer;
CD3DX12_CPU_DESCRIPTOR_HANDLE dx_renderer::screenRTV;

void dx_renderer::initialize(uint32 width, uint32 height)
{
	renderWidth = width;
	renderHeight = height;

	depthBuffer = createDepthTexture(&dxContext, width, height, DXGI_FORMAT_D32_FLOAT);
}

void dx_renderer::beginFrame(uint32 width, uint32 height, dx_resource screenBackbuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRTV)
{
	if (renderWidth != width || renderHeight != height)
	{
		renderWidth = width;
		renderHeight = height;

		resizeTexture(&dxContext, depthBuffer, width, height);
	}
}
