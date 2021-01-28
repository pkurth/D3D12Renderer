#pragma once

#include "dx.h"

struct dx_texture;

struct dx_render_target
{
	uint32 numAttachments;
	D3D12_VIEWPORT viewport;

	ref<dx_texture> colorAttachments[8];
	ref<dx_texture> depthAttachment;

	dx_render_target(std::initializer_list<ref<dx_texture>> colorAttachments, ref<dx_texture> depthAttachment = 0);
};
