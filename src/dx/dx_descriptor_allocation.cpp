#include "pch.h"
#include "dx_descriptor_allocation.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "dx_texture.h"
#include "core/block_allocator.h"


struct dx_descriptor_page
{
	dx_descriptor_page(D3D12_DESCRIPTOR_HEAP_TYPE type, uint64 capacity, bool shaderVisible);

	std::pair<dx_descriptor_allocation, bool> allocate(uint64 count);
	void free(dx_descriptor_allocation allocation);

	D3D12_DESCRIPTOR_HEAP_TYPE type;
	com<ID3D12DescriptorHeap> descriptorHeap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuBase;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuBase;

	uint32 descriptorSize;

	block_allocator allocator;
};

dx_descriptor_page::dx_descriptor_page(D3D12_DESCRIPTOR_HEAP_TYPE type, uint64 capacity, bool shaderVisible)
{
	allocator.initialize(capacity);

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = (uint32)capacity;
	desc.Type = type;
	if (shaderVisible)
	{
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	checkResult(dxContext.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	cpuBase = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuBase = shaderVisible ? descriptorHeap->GetGPUDescriptorHandleForHeapStart() : CD3DX12_GPU_DESCRIPTOR_HANDLE{};
	descriptorSize = dxContext.device->GetDescriptorHandleIncrementSize(type);
}

std::pair<dx_descriptor_allocation, bool> dx_descriptor_page::allocate(uint64 count)
{
	uint64 offset = allocator.allocate(count);
	if (offset == UINT64_MAX)
	{
		return { dx_descriptor_allocation{}, false };
	}

	dx_descriptor_allocation result;
	result.count = count;
	result.cpuBase.InitOffsetted(cpuBase, (uint32)offset, descriptorSize);
	result.gpuBase.InitOffsetted(gpuBase, (uint32)offset, descriptorSize);
	result.descriptorSize = descriptorSize;

	return { result, true };
}

void dx_descriptor_page::free(dx_descriptor_allocation allocation)
{
	uint64 offset = (allocation.cpuBase.ptr - cpuBase.ptr) / descriptorSize;
	allocator.free(offset, allocation.count);
}





void dx_descriptor_heap::initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible, uint64 pageSize)
{
	this->type = type;
	this->pageSize = pageSize;
	this->shaderVisible = shaderVisible;
}

dx_descriptor_allocation dx_descriptor_heap::allocate(uint64 count)
{
	if (count == 0)
	{
		return {};
	}

	assert(count <= pageSize);

	for (uint32 i = 0; i < (uint32)allPages.size(); ++i)
	{
		auto [allocation, success] = allPages[i]->allocate(count);
		if (success)
		{
			allocation.pageIndex = i;
			return allocation;
		}
	}

	uint32 pageIndex = (uint32)allPages.size();
	allPages.push_back(new dx_descriptor_page(type, pageSize, shaderVisible));
	
	auto [allocation, success] = allPages.back()->allocate(count);
	assert(success);
	allocation.pageIndex = pageIndex;

	return allocation;
}

void dx_descriptor_heap::free(dx_descriptor_allocation allocation)
{
	if (allocation.valid())
	{
		allPages[allocation.pageIndex]->free(allocation);
	}
}










struct dx_frame_descriptor_page
{
	com<ID3D12DescriptorHeap> descriptorHeap;
	dx_double_descriptor_handle base;
	uint32 usedDescriptors;
	uint32 maxNumDescriptors;
	uint32 descriptorHandleIncrementSize;

	dx_frame_descriptor_page* next;
};


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
		dx_frame_descriptor_page* page = usedPages[currentFrame];
		usedPages[currentFrame] = page->next;
		page->next = freePages;
		freePages = page;
	}

	mutex.unlock();
}

dx_descriptor_range dx_frame_descriptor_allocator::allocateContiguousDescriptorRange(uint32 count)
{
	mutex.lock();

	dx_frame_descriptor_page* current = usedPages[currentFrame];
	if (!current || (current->maxNumDescriptors - current->usedDescriptors < count))
	{
		dx_frame_descriptor_page* freePage = freePages;
		if (!freePage)
		{
			freePage = (dx_frame_descriptor_page*)calloc(1, sizeof(dx_frame_descriptor_page));

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

		freePages = freePage->next;
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

void dx_pushable_descriptor_heap::initialize(uint32 maxSize, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = maxSize;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	if (shaderVisible)
	{
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	checkResult(dxContext.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

	reset();
}

void dx_pushable_descriptor_heap::reset()
{
	currentCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	currentGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

dx_cpu_descriptor_handle dx_pushable_descriptor_heap::push()
{
	++currentGPU;
	return currentCPU++;
}
