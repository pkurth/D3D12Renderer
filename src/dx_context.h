#pragma once

#include "dx.h"
#include "dx_command_queue.h"
#include "dx_render_primitives.h"
#include "threading.h"
#include "memory.h"
#include "dx_upload_buffer.h"



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



	dx_factory factory;
	dx_adapter adapter;
	dx_device device;

	dx_command_queue renderQueue;
	dx_command_queue computeQueue;
	dx_command_queue copyQueue;

	bool raytracingSupported;

	uint64 frameID;
	uint32 bufferedFrameID;

	thread_mutex allocationMutex;
	memory_arena arena;

	object_retirement objectRetirement;

	dx_rtv_descriptor_heap rtvAllocator;
	dx_dsv_descriptor_heap dsvAllocator;

	dx_page_pool pagePools[NUM_BUFFERED_FRAMES];
	dx_frame_descriptor_allocator frameDescriptorAllocator;

	volatile bool running = true;


private:
	dx_command_queue& getQueue(D3D12_COMMAND_LIST_TYPE type);
	dx_command_list* getFreeCommandList(dx_command_queue& queue);
	dx_command_allocator* getFreeCommandAllocator(dx_command_queue& queue);
	dx_command_allocator* getFreeCommandAllocator(D3D12_COMMAND_LIST_TYPE type);

	dx_command_list* allocateCommandList(D3D12_COMMAND_LIST_TYPE type);
	dx_command_allocator* allocateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);

	void initializeBuffer(dx_buffer& buffer, uint32 elementSize, uint32 elementCount, void* data = 0, bool allowUnorderedAccess = false);
	void initializeDescriptorHeap(dx_descriptor_heap& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible = true);

};
