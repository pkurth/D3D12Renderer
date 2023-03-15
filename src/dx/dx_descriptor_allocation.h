#pragma once

#include "dx.h"
#include "dx_descriptor.h"
#include "core/threading.h"



struct dx_descriptor_page;

struct dx_descriptor_allocation
{
	inline CD3DX12_CPU_DESCRIPTOR_HANDLE cpuAt(uint32 index = 0) { ASSERT(index < count); return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, index, descriptorSize); }
	inline CD3DX12_GPU_DESCRIPTOR_HANDLE gpuAt(uint32 index = 0) { ASSERT(index < count); return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuBase, index, descriptorSize); }

	inline bool valid() { return count > 0; }
	uint64 count = 0;

private:
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuBase;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuBase;
	uint32 descriptorSize;
	uint32 pageIndex;

	friend struct dx_descriptor_page;
	friend struct dx_descriptor_heap;
};

struct dx_descriptor_heap
{
	void initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible, uint64 pageSize = 1024);

	dx_descriptor_allocation allocate(uint64 count = 1);
	void free(dx_descriptor_allocation allocation);

	D3D12_DESCRIPTOR_HEAP_TYPE type;

private:
	uint64 pageSize;
	bool shaderVisible;
	std::vector<dx_descriptor_page*> allPages;
};


struct dx_descriptor_range
{
	inline dx_double_descriptor_handle pushHandle()
	{
		dx_double_descriptor_handle result =
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE(base.cpuHandle, pushIndex, descriptorHandleIncrementSize) ,
			CD3DX12_GPU_DESCRIPTOR_HANDLE(base.gpuHandle, pushIndex, descriptorHandleIncrementSize) ,
		};
		++pushIndex;
		return result;
	}
	com<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	uint32 descriptorHandleIncrementSize;

private:
	dx_double_descriptor_handle base;

	uint32 maxNumDescriptors;

	uint32 pushIndex;

	friend struct dx_frame_descriptor_allocator;
};

struct dx_frame_descriptor_page;

struct dx_frame_descriptor_allocator
{
	dx_frame_descriptor_page* usedPages[NUM_BUFFERED_FRAMES];
	dx_frame_descriptor_page* freePages;
	uint32 currentFrame;

	std::mutex mutex;

	void initialize();
	void newFrame(uint32 bufferedFrameID);
	dx_descriptor_range allocateContiguousDescriptorRange(uint32 count);
};



struct dx_pushable_descriptor_heap
{
	void initialize(uint32 maxSize, bool shaderVisible = true);
	void reset();
	dx_cpu_descriptor_handle push();

	com<ID3D12DescriptorHeap> descriptorHeap;
	dx_cpu_descriptor_handle currentCPU;
	dx_gpu_descriptor_handle currentGPU;
};
