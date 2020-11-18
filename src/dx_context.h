#pragma once

#include "dx.h"
#include "dx_command_queue.h"
#include "dx_render_primitives.h"
#include "threading.h"
#include "memory.h"
#include "dx_upload_buffer.h"
#include "dx_descriptor_allocation.h"


struct object_retirement
{
	dx_object retiredObjects[NUM_BUFFERED_FRAMES][128];
	volatile uint32 numRetiredObjects[NUM_BUFFERED_FRAMES];
};

struct dx_context
{
	void initialize();
	void quit();

	void newFrame(uint64 frameID);
	void flushApplication();

	void retireObject(dx_object object);

	dx_command_list* getFreeCopyCommandList();
	dx_command_list* getFreeComputeCommandList(bool async);
	dx_command_list* getFreeRenderCommandList();
	uint64 executeCommandList(dx_command_list* commandList);

	// Careful with these functions. They are not thread safe.
	dx_allocation allocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	dx_dynamic_constant_buffer uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template <typename T> dx_dynamic_constant_buffer uploadDynamicConstantBuffer(const T& data)
	{
		return uploadDynamicConstantBuffer(sizeof(T), &data);
	}



	dx_factory factory;
	dx_adapter adapter;
	dx_device device;

	dx_command_queue renderQueue;
	dx_command_queue computeQueue;
	dx_command_queue copyQueue;

	bool raytracingSupported;
	bool meshShaderSupported;

	uint64 frameID;
	uint32 bufferedFrameID;

	dx_descriptor_heap descriptorAllocatorCPU; // Used for all kinds of resources.
	dx_descriptor_heap descriptorAllocatorGPU; // Used for resources, which can be UAV cleared.

	dx_rtv_descriptor_heap rtvAllocator;
	dx_dsv_descriptor_heap dsvAllocator;
	dx_upload_buffer frameUploadBuffer;

	dx_frame_descriptor_allocator frameDescriptorAllocator;

	volatile bool running = true;


private:
	dx_page_pool pagePools[NUM_BUFFERED_FRAMES];

	object_retirement objectRetirement;

	thread_mutex allocationMutex;

	dx_command_queue& getQueue(D3D12_COMMAND_LIST_TYPE type);
	dx_command_list* getFreeCommandList(dx_command_queue& queue);
};
