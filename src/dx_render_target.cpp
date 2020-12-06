#include "pch.h"
#include "dx_render_target.h"
#include "dx_texture.h"


uint32 dx_render_target::pushColorAttachment(const ref<dx_texture>& texture)
{
	assert(texture->resource);
	assert(texture->supportsRTV);

	uint32 attachmentPoint = numAttachments++;

	colorAttachments[attachmentPoint] = texture;

	if (width == 0 || height == 0)
	{
		width = texture->width;
		height = texture->height;
		viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
	}
	else
	{
		assert(width == texture->width && height == texture->height);
	}

	return attachmentPoint;
}

void dx_render_target::pushDepthStencilAttachment(const ref<dx_texture>& texture, uint32 arraySlice)
{
	assert(texture->resource);

	depthAttachment = texture;
	depthArraySlice = arraySlice;

	if (width == 0 || height == 0)
	{
		width = texture->width;
		height = texture->height;
		viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
	}
	else
	{
		assert(width == texture->width && height == texture->height);
	}
}

void dx_render_target::notifyOnTextureResize(uint32 width, uint32 height)
{
	this->width = width;
	this->height = height;
	viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
}
