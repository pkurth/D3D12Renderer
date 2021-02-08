#include "pch.h"
#include "dx_barrier_batcher.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "dx_descriptor_allocation.h"


barrier_batcher::barrier_batcher(dx_command_list* cl)
{
	this->cl = cl;
}

barrier_batcher& barrier_batcher::transition(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	if (from != to)
	{
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource);
	}
	return *this;
}

barrier_batcher& barrier_batcher::transition(const ref<dx_texture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (!res)
	{
		return *this;
	}

	return transition(res->resource, from, to, subresource);
}

barrier_batcher& barrier_batcher::transition(const ref<dx_buffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
	if (!res)
	{
		return *this;
	}

	return transition(res->resource, from, to);
}

barrier_batcher& barrier_batcher::transitionBegin(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	if (from != to)
	{
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource, D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY);
	}
	return *this;
}

barrier_batcher& barrier_batcher::transitionBegin(const ref<dx_texture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (!res)
	{
		return *this;
	}

	return transitionBegin(res->resource, from, to, subresource);
}

barrier_batcher& barrier_batcher::transitionBegin(const ref<dx_buffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
	if (!res)
	{
		return *this;
	}

	return transitionBegin(res->resource, from, to);
}

barrier_batcher& barrier_batcher::transitionEnd(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	if (from != to)
	{
		barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource, D3D12_RESOURCE_BARRIER_FLAG_END_ONLY);
	}
	return *this;
}

barrier_batcher& barrier_batcher::transitionEnd(const ref<dx_texture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (!res)
	{
		return *this;
	}

	return transitionEnd(res->resource, from, to, subresource);
}

barrier_batcher& barrier_batcher::transitionEnd(const ref<dx_buffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
	if (!res)
	{
		return *this;
	}

	return transitionEnd(res->resource, from, to);
}

barrier_batcher& barrier_batcher::uav(const dx_resource& resource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	return *this;
}

barrier_batcher& barrier_batcher::uav(const ref<dx_texture>& res)
{
	if (!res)
	{
		return *this;
	}

	return uav(res->resource);
}

barrier_batcher& barrier_batcher::uav(const ref<dx_buffer>& res)
{
	if (!res)
	{
		return *this;
	}

	return uav(res->resource);
}

barrier_batcher& barrier_batcher::aliasing(const dx_resource& before, const dx_resource& after)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	return *this;
}

void barrier_batcher::submit()
{
	if (numBarriers)
	{
		cl->barriers(barriers, numBarriers);
		numBarriers = 0;
	}
}
