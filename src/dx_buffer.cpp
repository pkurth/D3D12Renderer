#include "pch.h"
#include "dx_buffer.h"
#include "dx_command_list.h"
#include "dx_context.h"


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

void* mapBuffer(const ref<dx_buffer>& buffer)
{
	void* result;
	buffer->resource->Map(0, 0, &result);
	return result;
}

void unmapBuffer(const ref<dx_buffer>& buffer)
{
	buffer->resource->Unmap(0, 0);
}

void updateUploadBufferData(const ref<dx_buffer>& buffer, void* data, uint32 size)
{
	void* mapped = mapBuffer(buffer);
	memcpy(mapped, data, size);
	unmapBuffer(buffer);
}

static void uploadBufferData(ref<dx_buffer> buffer, const void* bufferData)
{
	dx_command_list* cl = dxContext.getFreeCopyCommandList();

	dx_resource intermediateResource;

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)));

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

static void updateBufferDataRange(ref<dx_buffer> buffer, const void* data, uint32 offset, uint32 size)
{
	assert(offset + size <= buffer->totalSize);

	dx_command_list* cl = dxContext.getFreeCopyCommandList();

	dx_resource intermediateResource;

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)));

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

static void initializeBuffer(ref<dx_buffer> buffer, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing,
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

	buffer->defaultSRV = {};
	buffer->defaultUAV = {};
	buffer->cpuClearUAV = {};
	buffer->gpuClearUAV = {};

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize, flags),
		initialState,
		0,
		IID_PPV_ARGS(&buffer->resource)));
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
			void* dataPtr = mapBuffer(buffer);
			memcpy(dataPtr, data, buffer->totalSize);
			unmapBuffer(buffer);
		}
	}

	if (buffer->supportsSRV)
	{
		buffer->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferSRV(buffer);
	}

	if (buffer->supportsUAV)
	{
		buffer->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferUAV(buffer);
	}

	if (buffer->supportsClearing)
	{
		buffer->cpuClearUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferUintUAV(buffer);
		dx_cpu_descriptor_handle shaderVisibleCPUHandle = dxContext.descriptorAllocatorGPU.getFreeHandle().createBufferUintUAV(buffer);
		buffer->gpuClearUAV = dxContext.descriptorAllocatorGPU.getMatchingGPUHandle(shaderVisibleCPUHandle);
	}
}

ref<dx_buffer> createBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing, initialState, D3D12_HEAP_TYPE_DEFAULT);
	return result;
}

ref<dx_buffer> createUploadBuffer(uint32 elementSize, uint32 elementCount, void* data)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, elementSize, elementCount, data, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	return result;
}

ref<dx_buffer> createReadbackBuffer(uint32 elementSize, uint32 elementCount, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_buffer> result = make_ref<dx_buffer>();
	initializeBuffer(result, elementSize, elementCount, 0, false, false, initialState, D3D12_HEAP_TYPE_READBACK);
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
	initializeBuffer(result, elementSize, elementCount, data, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
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

static void retire(dx_resource resource, dx_cpu_descriptor_handle srv, dx_cpu_descriptor_handle uav, dx_cpu_descriptor_handle clear, dx_gpu_descriptor_handle gpuClear)
{
	buffer_grave grave;
	grave.resource = resource;
	grave.srv = srv;
	grave.uav = uav;
	grave.clear = clear;
	if (gpuClear.gpuHandle.ptr)
	{
		grave.gpuClear = dxContext.descriptorAllocatorGPU.getMatchingCPUHandle(gpuClear);
	}
	else
	{
		grave.gpuClear = {};
	}
	dxContext.retire(std::move(grave));
}

dx_buffer::~dx_buffer()
{
	wchar name[128];

	if (resource)
	{
		uint32 size = sizeof(name);
		resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
		name[min(arraysize(name) - 1, size)] = 0;
	}


	retire(resource, defaultSRV, defaultUAV, cpuClearUAV, gpuClearUAV);
}

void resizeBuffer(ref<dx_buffer> buffer, uint32 newElementCount, D3D12_RESOURCE_STATES initialState)
{
	retire(buffer->resource, buffer->defaultSRV, buffer->defaultUAV, buffer->cpuClearUAV, buffer->gpuClearUAV);

	buffer->elementCount = newElementCount;
	buffer->totalSize = buffer->elementCount * buffer->elementSize;

	auto desc = buffer->resource->GetDesc();

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(buffer->heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer->totalSize, desc.Flags),
		initialState,
		0,
		IID_PPV_ARGS(&buffer->resource)));
	buffer->gpuVirtualAddress = buffer->resource->GetGPUVirtualAddress();


	if (buffer->supportsSRV)
	{
		buffer->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferSRV(buffer);
	}

	if (buffer->supportsUAV)
	{
		buffer->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferUAV(buffer);
	}

	if (buffer->supportsClearing)
	{
		buffer->cpuClearUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferUintUAV(buffer);
		dx_cpu_descriptor_handle shaderVisibleCPUHandle = dxContext.descriptorAllocatorGPU.getFreeHandle().createBufferUintUAV(buffer);
		buffer->gpuClearUAV = dxContext.descriptorAllocatorGPU.getMatchingGPUHandle(shaderVisibleCPUHandle);
	}
}

buffer_grave::~buffer_grave()
{
	if (resource)
	{
		//std::cout << "Finally deleting buffer." << std::endl;

		if (srv.cpuHandle.ptr)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(srv);
		}
		if (uav.cpuHandle.ptr)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(uav);
		}
		if (clear.cpuHandle.ptr)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(clear);
			dxContext.descriptorAllocatorGPU.freeHandle(gpuClear);
		}
	}
}
