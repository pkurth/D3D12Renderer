#pragma once

#include "dx.h"
#include "dx_descriptor.h"

struct dx_texture;

struct dx_render_target
{
	uint32 numAttachments;
	D3D12_VIEWPORT viewport;

	dx_rtv_descriptor_handle rtv[8];
	dx_dsv_descriptor_handle dsv;

	dx_render_target(std::initializer_list<ref<dx_texture>> colorAttachments, ref<dx_texture> depthAttachment = 0);
};
