#pragma once

#include "dx.h"
#include "dx_descriptor.h"
#include "core/threading.h"

template <typename descriptor_t>
struct dx_descriptor_heap
{
	void initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible);
	descriptor_t getFreeHandle();
	void freeHandle(descriptor_t handle);

	dx_gpu_descriptor_handle getMatchingGPUHandle(dx_cpu_descriptor_handle handle);
	dx_cpu_descriptor_handle getMatchingCPUHandle(dx_gpu_descriptor_handle handle);

protected:
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	com<ID3D12DescriptorHeap> descriptorHeap;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuBase;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuBase;
	uint32 descriptorHandleIncrementSize;
	std::vector<uint16> freeDescriptors;
	uint32 allFreeIncludingAndAfter;

	inline dx_cpu_descriptor_handle getHandle(uint32 index) { return { CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, index, descriptorHandleIncrementSize) }; }
};

template<typename descriptor_t>
inline void dx_descriptor_heap<descriptor_t>::initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible)
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

template<typename descriptor_t>
descriptor_t dx_descriptor_heap<descriptor_t>::getFreeHandle()
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

template<typename descriptor_t>
void dx_descriptor_heap<descriptor_t>::freeHandle(descriptor_t handle)
{
	uint32 index = (uint32)((handle.cpuHandle.ptr - cpuBase.ptr) / descriptorHandleIncrementSize);
	freeDescriptors.push_back(index);
}

template<typename descriptor_t>
dx_gpu_descriptor_handle dx_descriptor_heap<descriptor_t>::getMatchingGPUHandle(dx_cpu_descriptor_handle handle)
{
	uint32 offset = (uint32)(handle.cpuHandle.ptr - cpuBase.ptr);
	return { CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuBase, offset) };
}

template<typename descriptor_t>
dx_cpu_descriptor_handle dx_descriptor_heap<descriptor_t>::getMatchingCPUHandle(dx_gpu_descriptor_handle handle)
{
	uint32 offset = (uint32)(handle.gpuHandle.ptr - gpuBase.ptr);
	return { CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, offset) };
}








struct dx_descriptor_page
{
	com<ID3D12DescriptorHeap> descriptorHeap;
	dx_double_descriptor_handle base;
	uint32 usedDescriptors;
	uint32 maxNumDescriptors;
	uint32 descriptorHandleIncrementSize;

	dx_descriptor_page* next;
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

struct dx_frame_descriptor_allocator
{
	dx_descriptor_page* usedPages[NUM_BUFFERED_FRAMES];
	dx_descriptor_page* freePages;
	uint32 currentFrame;

	std::mutex mutex;

	void initialize();
	void newFrame(uint32 bufferedFrameID);
	dx_descriptor_range allocateContiguousDescriptorRange(uint32 count);
};



struct dx_pushable_resource_descriptor_heap
{
	void initialize(uint32 maxSize, bool shaderVisible = true);
	dx_cpu_descriptor_handle push();

	com<ID3D12DescriptorHeap> descriptorHeap;
	dx_cpu_descriptor_handle currentCPU;
	dx_gpu_descriptor_handle currentGPU;
};
