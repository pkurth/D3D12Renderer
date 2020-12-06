#pragma once

#include "dx.h"

struct dx_texture;

struct dx_render_target
{
	uint32 numAttachments;
	uint32 width;
	uint32 height;
	uint32 depthArraySlice;
	D3D12_VIEWPORT viewport;

	ref<dx_texture> colorAttachments[8];
	ref<dx_texture> depthAttachment;

	uint32 pushColorAttachment(const ref<dx_texture>& texture);
	void pushDepthStencilAttachment(const ref<dx_texture>& texture, uint32 arraySlice = 0);
	void notifyOnTextureResize(uint32 width, uint32 height); // This does NOT resize the textures, only updates the width, height and viewport variables.
};
