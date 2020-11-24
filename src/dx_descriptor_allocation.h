#pragma once

#include "dx.h"
#include "dx_descriptor.h"
#include "threading.h"

struct dx_descriptor_heap
{
	void initialize(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible);
	dx_cpu_descriptor_handle getFreeHandle();
	void freeHandle(dx_cpu_descriptor_handle handle);

	dx_gpu_descriptor_handle getMatchingGPUHandle(dx_cpu_descriptor_handle handle);
	dx_cpu_descriptor_handle getMatchingCPUHandle(dx_gpu_descriptor_handle handle);

	D3D12_DESCRIPTOR_HEAP_TYPE type;
	com<ID3D12DescriptorHeap> descriptorHeap;

protected:
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuBase;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuBase;
	uint32 descriptorHandleIncrementSize;
	std::vector<uint16> freeDescriptors;
	uint32 allFreeIncludingAndAfter;

	inline dx_cpu_descriptor_handle getHandle(uint32 index) { return { CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, index, descriptorHandleIncrementSize) }; }
};

struct dx_rtv_descriptor_heap : dx_descriptor_heap
{
	void initialize(uint32 numDescriptors, bool shaderVisible = true);

	volatile uint32 pushIndex;

	dx_cpu_descriptor_handle pushRenderTargetView(const ref<dx_texture>& texture);
	dx_cpu_descriptor_handle createRenderTargetView(const ref<dx_texture>& texture, dx_cpu_descriptor_handle index);
};

struct dx_dsv_descriptor_heap : dx_descriptor_heap
{
	void initialize(uint32 numDescriptors, bool shaderVisible = true);

	volatile uint32 pushIndex;

	dx_cpu_descriptor_handle pushDepthStencilView(const ref<dx_texture>& texture);
	dx_cpu_descriptor_handle createDepthStencilView(const ref<dx_texture>& texture, dx_cpu_descriptor_handle index);
};

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

	thread_mutex mutex;

	void initialize();
	void newFrame(uint32 bufferedFrameID);
	dx_descriptor_range allocateContiguousDescriptorRange(uint32 count);
};
