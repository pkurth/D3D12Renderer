#pragma once

#include "dx.h"
#include "dx_command_queue.h"
#include "dx_upload_buffer.h"
#include "dx_descriptor_allocation.h"
#include "dx_buffer.h"
#include "dx_query.h"
#include "core/threading.h"


struct dx_memory_usage
{
	// In MB.
	uint32 currentlyUsed;
	uint32 available;
};

enum dx_raytracing_tier
{
	dx_raytracing_not_available,
	dx_raytracing_1_0,
	dx_raytracing_1_1,
};

enum dx_mesh_shader_tier
{
	dx_mesh_shader_not_available,
	dx_mesh_shader_1_0,
};

struct dx_feature_support
{
	dx_raytracing_tier raytracingTier;
	dx_mesh_shader_tier meshShaderTier;

	bool raytracing() { return raytracingTier >= dx_raytracing_1_0; }
	bool meshShaders() { return meshShaderTier >= dx_mesh_shader_1_0; }
};

struct dx_context
{
	bool initialize();
	void quit();

	void newFrame(uint64 frameID);
	void endFrame(dx_command_list* cl);
	void flushApplication();

	dx_command_list* getFreeCopyCommandList();
	dx_command_list* getFreeComputeCommandList(bool async);
	dx_command_list* getFreeRenderCommandList();
	uint64 executeCommandList(dx_command_list* commandList);
	uint64 executeCommandLists(dx_command_list** commandLists, uint32 count);

	// Careful with these functions. They are not thread safe.
	dx_allocation allocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	dx_dynamic_constant_buffer uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template <typename T> dx_dynamic_constant_buffer uploadDynamicConstantBuffer(const T& data)
	{
		return uploadDynamicConstantBuffer(sizeof(T), &data);
	}

	dx_dynamic_vertex_buffer createDynamicVertexBuffer(uint32 elementSize, uint32 elementCount, const void* data);
	template <typename T> dx_dynamic_vertex_buffer createDynamicVertexBuffer(const T* data, uint32 count)
	{
		return createDynamicVertexBuffer((uint32)sizeof(T), count, data);
	}

	dx_memory_usage getMemoryUsage();


	dx_factory factory;
	dx_adapter adapter;
	dx_device device;

	dx_command_queue renderQueue;
	dx_command_queue computeQueue;
	dx_command_queue copyQueue;

	dx_feature_support featureSupport;

	uint64 frameID;
	uint32 bufferedFrameID;

	dx_descriptor_heap<dx_cpu_descriptor_handle> descriptorAllocatorCPU;
	dx_descriptor_heap<dx_cpu_descriptor_handle> descriptorAllocatorGPU; // Used for resources, which can be UAV cleared.

	dx_descriptor_heap<dx_rtv_descriptor_handle> rtvAllocator;
	dx_descriptor_heap<dx_dsv_descriptor_handle> dsvAllocator;

#if ENABLE_DX_PROFILING
	volatile uint32 timestampQueryIndex[NUM_BUFFERED_FRAMES];
#endif

	dx_upload_buffer frameUploadBuffer;

	dx_frame_descriptor_allocator frameDescriptorAllocator;

	volatile bool running = true;

	uint32 descriptorHandleIncrementSize;

	D3D12MA::Allocator* memoryAllocator;


	void retire(struct texture_grave&& texture);
	void retire(struct buffer_grave&& buffer);
	void retire(dx_object obj);
	void retire(D3D12MA::Allocation* allocation);

	dx_command_signature defaultDispatchCommandSignature;

private:
#if ENABLE_DX_PROFILING
	dx_timestamp_query_heap timestampHeaps[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> resolvedTimestampBuffers[NUM_BUFFERED_FRAMES];
#endif

	dx_page_pool pagePools[NUM_BUFFERED_FRAMES];

	std::mutex mutex;

	std::vector<struct texture_grave> textureGraveyard[NUM_BUFFERED_FRAMES];
	std::vector<struct buffer_grave> bufferGraveyard[NUM_BUFFERED_FRAMES];
	std::vector<dx_object> objectGraveyard[NUM_BUFFERED_FRAMES];
	std::vector<D3D12MA::Allocation*> allocationGraveyard[NUM_BUFFERED_FRAMES];

	dx_command_queue& getQueue(D3D12_COMMAND_LIST_TYPE type);
	dx_command_list* getFreeCommandList(dx_command_queue& queue);
};

extern dx_context& dxContext;
