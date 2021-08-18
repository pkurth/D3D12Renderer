#include "pch.h"
#include "dx_render_target.h"
#include "dx_texture.h"


dx_render_target::dx_render_target(std::initializer_list<ref<dx_texture>> colorAttachments, ref<dx_texture> depthAttachment)
{
	uint32 width = 0;
	uint32 height = 0;

	numAttachments = 0;
	for (const ref<dx_texture>& t : colorAttachments)
	{
		if (t)
		{
			width = t->width;
			height = t->height;
			rtv[numAttachments++] = t->defaultRTV;
		}
	}
	dsv = depthAttachment ? depthAttachment->defaultDSV : dx_dsv_descriptor_handle{ { 0 } };

	assert(numAttachments > 0 || depthAttachment != 0);

	width = (numAttachments > 0) ? width : depthAttachment->width;
	height = (numAttachments > 0) ? height : depthAttachment->height;

	viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
}
