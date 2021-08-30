#include "pch.h"
#include "dx_upload_buffer.h"
#include "core/memory.h"
#include "dx_context.h"

dx_page* dx_page_pool::allocateNewPage()
{
	mutex.lock();
	dx_page* result = arena.allocate<dx_page>(true);
	mutex.unlock();

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(pageSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&result->buffer)
	));

	result->gpuBasePtr = result->buffer->GetGPUVirtualAddress();
	result->buffer->Map(0, 0, (void**)&result->cpuBasePtr);
	result->pageSize = pageSize;

	return result;
}

dx_page* dx_page_pool::getFreePage()
{
	mutex.lock();
	dx_page* result = freePages;
	if (result)
	{
		freePages = result->next;
	}
	mutex.unlock();

	if (!result)
	{
		result = allocateNewPage();
	}

	result->currentOffset = 0;

	return result;
}

void dx_page_pool::returnPage(dx_page* page)
{
	mutex.lock();
	page->next = usedPages;
	usedPages = page;
	if (!lastUsedPage)
	{
		lastUsedPage = page;
	}
	mutex.unlock();
}

void dx_page_pool::reset()
{
	if (lastUsedPage)
	{
		lastUsedPage->next = freePages;
	}
	freePages = usedPages;
	usedPages = 0;
	lastUsedPage = 0;
}




dx_allocation dx_upload_buffer::allocate(uint64 size, uint64 alignment)
{
	assert(size <= pagePool->pageSize);

	uint64 alignedOffset = currentPage ? alignTo(currentPage->currentOffset, alignment) : 0;

	dx_page* page = currentPage;
	if (!page || alignedOffset + size > page->pageSize)
	{
		page = pagePool->getFreePage();
		alignedOffset = 0;

		if (currentPage)
		{
			pagePool->returnPage(currentPage);
		}
		currentPage = page;
	}

	dx_allocation result;
	result.cpuPtr = page->cpuBasePtr + alignedOffset;
	result.gpuPtr = page->gpuBasePtr + alignedOffset;
	result.resource = page->buffer;
	result.offsetInResource = (uint32)alignedOffset;

	page->currentOffset = alignedOffset + size;

	return result;
}

void dx_upload_buffer::reset()
{
	if (currentPage && pagePool)
	{
		pagePool->returnPage(currentPage);
	}
	currentPage = 0;
}

void dx_page_pool::initialize(uint32 sizeInBytes)
{
	pageSize = sizeInBytes;
	arena.initialize(0, sizeof(dx_page) * 512);
}

