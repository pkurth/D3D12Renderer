#include "pch.h"
#include "dx_descriptor_allocation.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "dx_texture.h"



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

void dx_pushable_resource_descriptor_heap::initialize(uint32 maxSize, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = maxSize;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	if (shaderVisible)
	{
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	checkResult(dxContext.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	currentCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	currentGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

dx_cpu_descriptor_handle dx_pushable_resource_descriptor_heap::push()
{
	++currentGPU;
	return currentCPU++;
}


