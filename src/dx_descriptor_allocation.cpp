#include "pch.h"
#include "dx_descriptor_allocation.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "dx_texture.h"


void dx_descriptor_heap::initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;
	if (shaderVisible)
	{
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	checkResult(dxContext.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	allFreeIncludingAndAfter = 0;
	descriptorHandleIncrementSize = dxContext.device->GetDescriptorHandleIncrementSize(type);
	cpuBase = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuBase = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	this->type = type;
}

dx_cpu_descriptor_handle dx_descriptor_heap::getFreeHandle()
{
	uint32 index;
	if (!freeDescriptors.empty())
	{
		index = freeDescriptors.back();
		freeDescriptors.pop_back();
	}
	else
	{
		index = atomicAdd(allFreeIncludingAndAfter, 1);
	}
	return { CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, index, descriptorHandleIncrementSize) };
}

void dx_descriptor_heap::freeHandle(dx_cpu_descriptor_handle handle)
{
	uint32 index = (uint32)((handle.cpuHandle.ptr - cpuBase.ptr) / descriptorHandleIncrementSize);
	freeDescriptors.push_back(index);
}

dx_gpu_descriptor_handle dx_descriptor_heap::getMatchingGPUHandle(dx_cpu_descriptor_handle handle)
{
	uint32 offset = (uint32)(handle.cpuHandle.ptr - cpuBase.ptr);
	return { CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuBase, offset) };
}

dx_cpu_descriptor_handle dx_descriptor_heap::getMatchingCPUHandle(dx_gpu_descriptor_handle handle)
{
	uint32 offset = (uint32)(handle.gpuHandle.ptr - gpuBase.ptr);
	return { CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, offset) };
}


void dx_rtv_descriptor_heap::initialize(uint32 numDescriptors, bool shaderVisible)
{
	pushIndex = 0;
	dx_descriptor_heap::initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, shaderVisible);
}

dx_cpu_descriptor_handle dx_rtv_descriptor_heap::pushRenderTargetView(const ref<dx_texture>& texture)
{
	uint32 slices = texture->depth;

	uint32 index = atomicAdd(pushIndex, slices);
	dx_cpu_descriptor_handle result = getHandle(index);
	return createRenderTargetView(texture, result);
}

dx_cpu_descriptor_handle dx_rtv_descriptor_heap::createRenderTargetView(const ref<dx_texture>& texture, dx_cpu_descriptor_handle index)
{
	assert(texture->supportsRTV);

	uint32 slices = texture->depth;
	if (slices > 1)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = texture->format;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		for (uint32 i = 0; i < slices; ++i)
		{
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(index.cpuHandle, i, descriptorHandleIncrementSize);
			dxContext.device->CreateRenderTargetView(texture->resource.Get(), &rtvDesc, rtv);
		}
	}
	else
	{
		dxContext.device->CreateRenderTargetView(texture->resource.Get(), 0, index.cpuHandle);
	}

	return index;
}

void dx_dsv_descriptor_heap::initialize(uint32 numDescriptors, bool shaderVisible)
{
	pushIndex = 0;
	dx_descriptor_heap::initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, shaderVisible);
}

dx_cpu_descriptor_handle dx_dsv_descriptor_heap::pushDepthStencilView(const ref<dx_texture>& texture)
{
	uint32 slices = texture->depth;
	uint32 index = atomicAdd(pushIndex, slices);
	dx_cpu_descriptor_handle result = getHandle(index);
	return createDepthStencilView(texture, result);
}

dx_cpu_descriptor_handle dx_dsv_descriptor_heap::createDepthStencilView(const ref<dx_texture>& texture, dx_cpu_descriptor_handle index)
{
	DXGI_FORMAT format = texture->format;
	uint32 slices = texture->depth;

	assert(isDepthFormat(format));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	if (slices > 1)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.ArraySize = 1;

		for (uint32 i = 0; i < slices; ++i)
		{
			dsvDesc.Texture2DArray.FirstArraySlice = i;

			dx_cpu_descriptor_handle handle = { CD3DX12_CPU_DESCRIPTOR_HANDLE(index.cpuHandle, i, dxContext.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)) };
			dxContext.device->CreateDepthStencilView(texture->resource.Get(), &dsvDesc, handle);
		}
	}
	else
	{
		dxContext.device->CreateDepthStencilView(texture->resource.Get(), &dsvDesc, index.cpuHandle);
	}

	return index;
}

void dx_frame_descriptor_allocator::initialize()
{
	currentFrame = NUM_BUFFERED_FRAMES - 1;
}

void dx_frame_descriptor_allocator::newFrame(uint32 bufferedFrameID)
{
	mutex.lock();

	currentFrame = bufferedFrameID;

	while (usedPages[currentFrame])
	{
		dx_descriptor_page* page = usedPages[currentFrame];
		usedPages[currentFrame] = page->next;
		page->next = freePages;
		freePages = page;
	}

	mutex.unlock();
}

dx_descriptor_range dx_frame_descriptor_allocator::allocateContiguousDescriptorRange(uint32 count)
{
	mutex.lock();

	dx_descriptor_page* current = usedPages[currentFrame];
	if (!current || (current->maxNumDescriptors - current->usedDescriptors < count))
	{
		dx_descriptor_page* freePage = freePages;
		if (!freePage)
		{
			freePage = (dx_descriptor_page*)calloc(1, sizeof(dx_descriptor_page));

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = 1024;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			checkResult(dxContext.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&freePage->descriptorHeap)));

			freePage->maxNumDescriptors = desc.NumDescriptors;
			freePage->descriptorHandleIncrementSize = dxContext.device->GetDescriptorHandleIncrementSize(desc.Type);
			freePage->base.cpuHandle = freePage->descriptorHeap->GetCPUDescriptorHandleForHeapStart();
			freePage->base.gpuHandle = freePage->descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		freePage->usedDescriptors = 0;
		freePage->next = current;
		usedPages[currentFrame] = freePage;
		current = freePage;
	}

	uint32 index = current->usedDescriptors;
	current->usedDescriptors += count;

	mutex.unlock();

	dx_descriptor_range result;

	result.base.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(current->base.cpuHandle, index, current->descriptorHandleIncrementSize);
	result.base.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(current->base.gpuHandle, index, current->descriptorHandleIncrementSize);
	result.descriptorHandleIncrementSize = current->descriptorHandleIncrementSize;
	result.descriptorHeap = current->descriptorHeap;
	result.maxNumDescriptors = count;
	result.pushIndex = 0;

	return result;
}



