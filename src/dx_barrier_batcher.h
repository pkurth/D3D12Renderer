#pragma once

#include "dx.h"

struct dx_texture;
struct dx_buffer;
struct dx_command_list;

struct barrier_batcher
{
	barrier_batcher(dx_command_list* cl);
	~barrier_batcher() { submit(); }


	barrier_batcher& transition(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& transition(const ref<dx_texture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& transition(const ref<dx_buffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

	// Split barriers.
	barrier_batcher& transitionBegin(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& transitionBegin(const ref<dx_texture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& transitionBegin(const ref<dx_buffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

	barrier_batcher& transitionEnd(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& transitionEnd(const ref<dx_texture>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& transitionEnd(const ref<dx_buffer>& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);



	barrier_batcher& uav(const dx_resource& res);
	barrier_batcher& uav(const ref<dx_texture>& res);
	barrier_batcher& uav(const ref<dx_buffer>& res);

	barrier_batcher& aliasing(const dx_resource& before, const dx_resource& after);

	void submit();

	dx_command_list* cl;
	CD3DX12_RESOURCE_BARRIER barriers[16];
	uint32 numBarriers = 0;
};


