#include "pch.h"
#include "dx_descriptor_allocation.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "dx_texture.h"

#include <map>


// Based on https://www.codeproject.com/Articles/1180070/Simple-Variable-Size-Memory-Block-Allocator.
struct block_allocator
{
	uint64 availableSize;

	void initialize(uint64 capacity);

	uint64 allocate(uint64 requestedSize);
	void free(uint64 offset, uint64 size);

private:

	struct offset_value;
	struct size_value;

	// Referencing each-other.
	using block_by_offset_map = std::map<uint64, offset_value>;
	using block_by_size_map = std::multimap<uint64, size_value>;

	struct offset_value
	{
		offset_value() {}
		offset_value(block_by_size_map::iterator sizeIterator) : sizeIterator(sizeIterator) {}
		block_by_size_map::iterator sizeIterator;
		uint64 getSize() { return sizeIterator->first; }
	};

	struct size_value
	{
		size_value() {}
		size_value(block_by_offset_map::iterator offsetIterator) : offsetIterator(offsetIterator) {}
		block_by_offset_map::iterator offsetIterator;
		uint64 getOffset() { return offsetIterator->first; }
	};

	block_by_offset_map blocksByOffset;
	block_by_size_map blocksBySize;


	void addNewBlock(uint64 offset, uint64 size)
	{
		auto newBlockIt = blocksByOffset.emplace(offset, offset_value());
		auto orderIt = blocksBySize.emplace(size, newBlockIt.first);
		newBlockIt.first->second.sizeIterator = orderIt;
	}
};

void block_allocator::initialize(uint64 capacity)
{
	availableSize = capacity;
	addNewBlock(0, capacity);
}

uint64 block_allocator::allocate(uint64 requestedSize)
{
	auto smallestBlockItIt = blocksBySize.lower_bound(requestedSize);
	if (smallestBlockItIt == blocksBySize.end())
	{
		return UINT64_MAX;
	}

	size_value sizeValue = smallestBlockItIt->second;

	uint64 offset = sizeValue.getOffset();
	uint64 size = smallestBlockItIt->first;

	uint64 newOffset = offset + requestedSize;
	uint64 newSize = size - requestedSize;

	blocksBySize.erase(smallestBlockItIt);
	blocksByOffset.erase(sizeValue.offsetIterator);

	if (newSize > 0)
	{
		addNewBlock(newOffset, newSize);
	}

	availableSize -= requestedSize;
	return offset;
}

void block_allocator::free(uint64 offset, uint64 size)
{
	auto nextBlockIt = blocksByOffset.upper_bound(offset);
	auto prevBlockIt = nextBlockIt;
	if (prevBlockIt != blocksByOffset.begin())
	{
		--prevBlockIt;
	}
	else
	{
		prevBlockIt = blocksByOffset.end();
	}

	uint64 newOffset, newSize;
	if (prevBlockIt != blocksByOffset.end() && offset == prevBlockIt->first + prevBlockIt->second.getSize())
	{
		// PrevBlock.Offset           Offset
		// |                          |
		// |<-----PrevBlock.Size----->|<------Size-------->|
		//
		newSize = prevBlockIt->second.getSize() + size;
		newOffset = prevBlockIt->first;

		if (nextBlockIt != blocksByOffset.end() && offset + size == nextBlockIt->first)
		{
			// PrevBlock.Offset           Offset               NextBlock.Offset
			// |                          |                    |
			// |<-----PrevBlock.Size----->|<------Size-------->|<-----NextBlock.Size----->|
			//
			newSize += nextBlockIt->second.getSize();
			blocksBySize.erase(prevBlockIt->second.sizeIterator);
			blocksBySize.erase(nextBlockIt->second.sizeIterator);
			// Delete the range of two blocks
			++nextBlockIt;
			blocksByOffset.erase(prevBlockIt, nextBlockIt);
		}
		else
		{
			// PrevBlock.Offset           Offset                       NextBlock.Offset
			// |                          |                            |
			// |<-----PrevBlock.Size----->|<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
			//
			blocksBySize.erase(prevBlockIt->second.sizeIterator);
			blocksByOffset.erase(prevBlockIt);
		}
	}
	else if (nextBlockIt != blocksByOffset.end() && offset + size == nextBlockIt->first)
	{
		// PrevBlock.Offset                   Offset               NextBlock.Offset
		// |                                  |                    |
		// |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->|<-----NextBlock.Size----->|
		//
		newSize = size + nextBlockIt->second.getSize();
		newOffset = offset;
		blocksBySize.erase(nextBlockIt->second.sizeIterator);
		blocksByOffset.erase(nextBlockIt);
	}
	else
	{
		// PrevBlock.Offset                   Offset                       NextBlock.Offset
		// |                                  |                            |
		// |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
		//
		newSize = size;
		newOffset = offset;
	}

	addNewBlock(newOffset, newSize);

	availableSize += size;
}



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

	currentCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	currentGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

dx_cpu_descriptor_handle dx_pushable_descriptor_heap::push()
{
	++currentGPU;
	return currentCPU++;
}
