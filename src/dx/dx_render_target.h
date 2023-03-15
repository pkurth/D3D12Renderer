#pragma once

#include "dx.h"
#include "dx_descriptor.h"
#include "dx_texture.h"

struct dx_texture;

struct dx_render_target
{
	uint32 numAttachments = 0;
	D3D12_VIEWPORT viewport;

	dx_rtv_descriptor_handle rtv[8];
	dx_dsv_descriptor_handle dsv = CD3DX12_DEFAULT();


	dx_render_target(uint32 width, uint32 height)
	{
		viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
	}

	dx_render_target& colorAttachment(const ref<dx_texture>& attachment, dx_rtv_descriptor_handle useIfNull = CD3DX12_DEFAULT())
	{
		ASSERT(pushIndex < arraysize(rtv));
		rtv[pushIndex++] = attachment ? attachment->defaultRTV : useIfNull;

		if (attachment)
		{
			// Change numAttachments only, if attachment is valid. This way the invalid attachments at the end are ignored, when the render target is bound to the command list.
			numAttachments = pushIndex;
		}

		return *this;
	}

	dx_render_target& depthAttachment(const ref<dx_texture>& attachment, dx_dsv_descriptor_handle useIfNull = CD3DX12_DEFAULT())
	{
		ASSERT(!dsv);
		dsv = attachment ? attachment->defaultDSV : useIfNull;
		return *this;
	}

private:
	uint32 pushIndex = 0;
};
