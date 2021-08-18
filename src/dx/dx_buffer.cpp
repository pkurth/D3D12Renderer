#include "pch.h"
#include "dx_buffer.h"
#include "dx_command_list.h"
#include "dx_context.h"

#include <d3d12memoryallocator/D3D12MemAlloc.h>

DXGI_FORMAT getIndexBufferFormat(uint32 elementSize)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if (elementSize == 1)
	{
		result = DXGI_FORMAT_R8_UINT;
	}
	else if (elementSize == 2)
	{
		result = DXGI_FORMAT_R16_UINT;
	}
	else if (elementSize == 4)
	{
		result = DXGI_FORMAT_R32_UINT;
	}
	return result;
}

void* mapBuffer(const ref<dx_buffer>& buffer, bool intentsReading, map_range readRange)
{
	D3D12_RANGE range = { 0, 0 };
	D3D12_RANGE* r = 0;

	if (intentsReading)
	{
		if (readRange.numElements != -1)
		{
			range.Begin = readRange.firstElement * buffer->elementSize;
			range.End = range.Begin + readRange.numElements * buffer->elementSize;
			r = &range;
		}
	}
	else
	{
		r = &range;
	}

	void* result;
	buffer->resource->Map(0, r, &result);
	return result;
}

void unmapBuffer(const ref<dx_buffer>& buffer, bool hasWritten, map_range writtenRange)
{
	D3D12_RANGE range = { 0, 0 };
	D3D12_RANGE* r = 0;

	if (hasWritten)
	{
		if (writtenRange.numElements != -1)
		{
			range.Begin = writtenRange.firstElement * buffer->elementSize;
			range.End = range.Begin + writtenRange.numElements * buffer->elementSize;
			r = &range;
		}
	}
	else
	{
		r = &range;
	}

	buffer->resource->Unmap(0, r);
}

void updateUploadBufferData(const ref<dx_buffer>& buffer, void* data, uint32 size)
{
	void* mapped = mapBuffer(buffer, false);
	memcpy(mapped, data, size);
	unmapBuffer(buffer, true);
}

static void uploadBufferData(ref<dx_buffer> buffer, const void* bufferData)
{
	dx_command_list* cl = dxContext.getFreeCopyCommandList();

	dx_resource intermediateResource;

#if !USE_D3D12_BLOCK_ALLOCATOR
	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		&allocation,
		IID_PPV_ARGS(&intermediateResource)));

	dxContext.retire(allocation);
#endif

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = bufferData;
	subresourceData.RowPitch = buffer->totalSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	cl->transitionBarrier(buffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	UpdateSubresources(cl->commandList.Get(),
		buffer->resource.Get(), intermediateResource.Get(),
		0, 0, 1, &subresourceData);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.

	dxContext.retire(intermediateResource);
	dxContext.executeCommandList(cl);
}

void updateBufferDataRange(ref<dx_buffer> buffer, const void* data, uint32 offset, uint32 size)
{
	assert(offset + size <= buffer->totalSize);

	dx_command_list* cl = dxContext.getFreeCopyCommandList();

	dx_resource intermediateResource;

#if !USE_D3D12_BLOCK_ALLOCATOR
	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		&allocation,
		IID_PPV_ARGS(&intermediateResource)));

	dxContext.retire(allocation);
#endif

	cl->transitionBarrier(buffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	void* mapped;
	checkResult(intermediateResource->Map(0, 0, &mapped));
	memcpy(mapped, data, size);
	intermediateResource->Unmap(0, 0);

	cl->commandList->CopyBufferRegion(buffer->resource.Get(), offset, intermediateResource.Get(), 0, size);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.

	dxContext.retire(intermediateResource);
	dxContext.executeCommandList(cl);
}

static void initializeBuffer(ref<dx_buffer> buffer, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, bool raytracing = false,
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT)
{
	D3D12_RESOURCE_FLAGS flags = allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	buffer->elementSize = elementSize;
	buffer->elementCount = elementCount;
	buffer->totalSize = elementSize * elementCount;
	buffer->heapType = heapType;
	buffer->supportsSRV = heapType != D3D12_HEAP_TYPE_READBACK;
	buffer->supportsUAV = allowUnorderedAccess;
	buffer->supportsClearing = allowClearing;
	buffer->raytracing = raytracing;

	buffer->defaultSRV = {};
	buffer->defaultUAV = {};
	buffer->cpuClearUAV = {};
	buffer->gpuClearUAV = {};
	buffer->raytracingSRV = {};

#if !USE_D3D12_BLOCK_ALLOCATOR
	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize, flags),
		initialState,
		0,
		IID_PPV_ARGS(&buffer->resource)));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = heapType;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize, flags),
		initialState,
		0,
		&allocation,
		IID_PPV_ARGS(&buffer->resource)));

	buffer->allocation = allocation;
#endif

	buffer->gpuVirtualAddress = buffer->resource->GetGPUVirtualAddress();

	// Upload.
	if (data)
	{
		if (heapType == D3D12_HEAP_TYPE_DEFAULT)
		{
			uploadBufferData(buffer, data);
		}
		else if (heapType == D3D12_HEAP_TYPE_UPLOAD)
		{
			void* dataPtr = mapBuffer(buffer, false);
			memcpy(dataPtr, data, buffer->totalSize);
			unmapBuffer(buffer, true);
		}
	}

	uint32 numDescriptors = buffer->supportsSRV + buffer->supportsUAV + buffer->supportsClearing + raytracing;
	buffer->descriptorAllocation = dxContext.srvUavAllocator.allocate(numDescriptors);

	uint32 index = 0;

	if (buffer->supportsSRV)
	{
		buffer->defaultSRV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createBufferSRV(buffer);
	}

	if (buffer->supportsUAV)
	{
		buffer->defaultUAV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createBufferUAV(buffer);
	}

	if (buffer->supportsClearing)
	{
		buffer->cpuClearUAV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createBufferUintUAV(buffer);

		buffer->shaderVisibleDescriptorAllocation = dxContext.srvUavAllocatorShaderVisible.allocate(1);
		dx_cpu_descriptor_handle(buffer->shaderVisibleDescriptorAllocation.cpuAt(0)).createBufferUintUAV(buffer);
		buffer->gpuClearUAV = buffer->shaderVisibleDescriptorAllocation.gpuAt(0);
	}

	if (raytracing)
	{
		buffer->raytracingSRV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createRaytracingAccelerationStructureSRV(buffer);
	}

	assert(index == numDescriptors);
}

ref<dx_buffer> createBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing, false, initialState, D3D12_HEAP_TYPE_DEFAULT);
	return result;
}

ref<dx_buffer> createUploadBuffer(uint32 elementSize, uint32 elementCount, void* data)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, false, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	return result;
}

ref<dx_buffer> createReadbackBuffer(uint32 elementSize, uint32 elementCount, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, elementSize, elementCount, 0, false, false, false, initialState, D3D12_HEAP_TYPE_READBACK);
	return result;
}

ref<dx_vertex_buffer> createVertexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing)
{
	ref<dx_vertex_buffer> result = make_ref<dx_vertex_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result->view.BufferLocation = result->gpuVirtualAddress;
	result->view.SizeInBytes = result->totalSize;
	result->view.StrideInBytes = elementSize;
	return result;
}

ref<dx_vertex_buffer> createUploadVertexBuffer(uint32 elementSize, uint32 elementCount, void* data)
{
	ref<dx_vertex_buffer> result = make_ref<dx_vertex_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, false, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	result->view.BufferLocation = result->gpuVirtualAddress;
	result->view.SizeInBytes = result->totalSize;
	result->view.StrideInBytes = elementSize;
	return result;
}

ref<dx_index_buffer> createIndexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing)
{
	ref<dx_index_buffer> result = make_ref<dx_index_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result->view.BufferLocation = result->gpuVirtualAddress;
	result->view.SizeInBytes = result->totalSize;
	result->view.Format = getIndexBufferFormat(elementSize);
	return result;
}

ref<dx_buffer> createRaytracingTLASBuffer(uint32 size)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, size, 1, nullptr, true, false, true, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	return result;
}

static void retire(dx_resource resource, dx_descriptor_allocation descriptorAllocation, dx_descriptor_allocation shaderVisibleDescriptorAllocation)
{
	buffer_grave grave;
	grave.resource = resource;
	grave.descriptorAllocation = descriptorAllocation;
	grave.shaderVisibleDescriptorAllocation = shaderVisibleDescriptorAllocation;
	dxContext.retire(std::move(grave));
}

dx_buffer::~dx_buffer()
{
	retire(resource, descriptorAllocation, shaderVisibleDescriptorAllocation);
	if (allocation)
	{
		dxContext.retire(allocation);
	}
}

void resizeBuffer(ref<dx_buffer> buffer, uint32 newElementCount, D3D12_RESOURCE_STATES initialState)
{
	retire(buffer->resource, buffer->descriptorAllocation, buffer->shaderVisibleDescriptorAllocation);
	if (buffer->allocation)
	{
		dxContext.retire(buffer->allocation);
	}

	buffer->elementCount = newElementCount;
	buffer->totalSize = buffer->elementCount * buffer->elementSize;

	auto desc = buffer->resource->GetDesc();

#if !USE_D3D12_BLOCK_ALLOCATOR
	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(buffer->heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize, desc.Flags),
		initialState,
		0,
		IID_PPV_ARGS(&buffer->resource)));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = buffer->heapType;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize, desc.Flags),
		initialState,
		0,
		&allocation,
		IID_PPV_ARGS(&buffer->resource)));

	buffer->allocation = allocation;
#endif

	buffer->gpuVirtualAddress = buffer->resource->GetGPUVirtualAddress();


	uint32 numDescriptors = buffer->supportsSRV + buffer->supportsUAV + buffer->supportsClearing + buffer->raytracing;
	buffer->descriptorAllocation = dxContext.srvUavAllocator.allocate(numDescriptors);

	uint32 index = 0;

	if (buffer->supportsSRV)
	{
		buffer->defaultSRV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createBufferSRV(buffer);
	}

	if (buffer->supportsUAV)
	{
		buffer->defaultUAV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createBufferUAV(buffer);
	}

	if (buffer->supportsClearing)
	{
		buffer->cpuClearUAV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createBufferUintUAV(buffer);

		buffer->shaderVisibleDescriptorAllocation = dxContext.srvUavAllocatorShaderVisible.allocate(1);
		dx_cpu_descriptor_handle(buffer->shaderVisibleDescriptorAllocation.cpuAt(0)).createBufferUintUAV(buffer);
		buffer->gpuClearUAV = buffer->shaderVisibleDescriptorAllocation.gpuAt(0);
	}

	if (buffer->raytracing)
	{
		buffer->raytracingSRV = dx_cpu_descriptor_handle(buffer->descriptorAllocation.cpuAt(index++)).createRaytracingAccelerationStructureSRV(buffer);
	}

	assert(index == numDescriptors);
}

buffer_grave::~buffer_grave()
{
	if (resource)
	{
		dxContext.srvUavAllocator.free(descriptorAllocation);
		dxContext.srvUavAllocatorShaderVisible.free(shaderVisibleDescriptorAllocation);
	}
}
