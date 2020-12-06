#pragma once

#include "dx.h"
#include "memory.h"

struct dx_allocation
{
	void* cpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;
};

struct dx_page
{
	dx_resource buffer;
	dx_page* next;

	uint8* cpuBasePtr;
	D3D12_GPU_VIRTUAL_ADDRESS gpuBasePtr;

	uint64 pageSize;
	uint64 currentOffset;
};

struct dx_page_pool
{
	void initialize(uint32 sizeInBytes);

	memory_arena arena;

	std::mutex mutex;

	uint64 pageSize;
	dx_page* freePages;
	dx_page* usedPages;
	dx_page* lastUsedPage;

	dx_page* getFreePage();
	void returnPage(dx_page* page);
	void reset();

private:
	dx_page* allocateNewPage();
};

struct dx_upload_buffer
{
	dx_page_pool* pagePool = 0;
	dx_page* currentPage = 0;

	dx_allocation allocate(uint64 size, uint64 alignment);
	void reset();
};
