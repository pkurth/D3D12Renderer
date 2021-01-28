#include "pch.h"
#include "dx_render_target.h"
#include "dx_texture.h"


dx_render_target::dx_render_target(std::initializer_list<ref<dx_texture>> colorAttachments, ref<dx_texture> depthAttachment)
{
	numAttachments = 0;
	for (const ref<dx_texture>& t : colorAttachments)
	{
		if (t)
		{
			this->colorAttachments[numAttachments++] = t;
		}
	}
	this->depthAttachment = depthAttachment;

	assert(numAttachments > 0 || depthAttachment != 0);

	uint32 width = (numAttachments > 0) ? this->colorAttachments[0]->width : depthAttachment->width;
	uint32 height = (numAttachments > 0) ? this->colorAttachments[0]->height : depthAttachment->height;

	viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
}
