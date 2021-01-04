#pragma once

#include "dx_descriptor.h"


struct dx_dynamic_constant_buffer
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;
	void* cpuPtr;
};

struct dx_dynamic_vertex_buffer
{
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_buffer
{
	virtual ~dx_buffer();

	dx_resource resource;

	dx_cpu_descriptor_handle defaultSRV;
	dx_cpu_descriptor_handle defaultUAV;

	dx_cpu_descriptor_handle cpuClearUAV;
	dx_gpu_descriptor_handle gpuClearUAV;

	D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress;

	bool supportsUAV;
	bool supportsSRV;
	bool supportsClearing;

	uint32 elementSize = 0;
	uint32 elementCount = 0;
	uint32 totalSize = 0;
	D3D12_HEAP_TYPE heapType;
};

struct dx_vertex_buffer : dx_buffer
{
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_index_buffer : dx_buffer
{
	D3D12_INDEX_BUFFER_VIEW view;
};

struct dx_mesh
{
	ref<dx_vertex_buffer> vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
};

struct buffer_grave
{
	dx_resource resource;

	dx_cpu_descriptor_handle srv;
	dx_cpu_descriptor_handle uav;
	dx_cpu_descriptor_handle clear;
	dx_cpu_descriptor_handle gpuClear;

	buffer_grave() {}
	buffer_grave(const buffer_grave& o) = delete;
	buffer_grave(buffer_grave&& o) = default;

	buffer_grave& operator=(const buffer_grave& o) = delete;
	buffer_grave& operator=(buffer_grave&& o) = default;

	~buffer_grave();
};

DXGI_FORMAT getIndexBufferFormat(uint32 elementSize);

void* mapBuffer(const ref<dx_buffer>& buffer);
void unmapBuffer(const ref<dx_buffer>& buffer);
void uploadBufferData(ref<dx_buffer> buffer, const void* bufferData);
void updateBufferDataRange(ref<dx_buffer> buffer, const void* data, uint32 offset, uint32 size);

ref<dx_buffer> createBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
ref<dx_buffer> createUploadBuffer(uint32 elementSize, uint32 elementCount, void* data);
ref<dx_buffer> createReadbackBuffer(uint32 elementSize, uint32 elementCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST);
ref<dx_vertex_buffer> createVertexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
ref<dx_vertex_buffer> createUploadVertexBuffer(uint32 elementSize, uint32 elementCount, void* data);
ref<dx_index_buffer> createIndexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);

void resizeBuffer(ref<dx_buffer> buffer, uint32 newElementCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
