#pragma once

#include "dx.h"
#include "dx_upload_buffer.h"
#include "dx_dynamic_descriptor_heap.h"
#include "dx_descriptor_allocation.h"
#include "dx_texture.h"
#include "dx_buffer.h"
#include "dx_render_target.h"
#include "dx_pipeline.h"

struct dx_command_list
{
	dx_command_list(D3D12_COMMAND_LIST_TYPE type);

	D3D12_COMMAND_LIST_TYPE type;
	dx_command_allocator commandAllocator;
	dx_graphics_command_list commandList;
	uint64 lastExecutionFenceValue;
	dx_command_list* next;

	dx_upload_buffer uploadBuffer;
	dx_dynamic_descriptor_heap dynamicDescriptorHeap;


	ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};


	// Barriers.
	void barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers);

	void transitionBarrier(const ref<dx_texture>& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void transitionBarrier(const ref<dx_buffer>& buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void transitionBarrier(dx_resource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void uavBarrier(const ref<dx_texture>& texture);
	void uavBarrier(const ref<dx_buffer>& buffer);
	void uavBarrier(dx_resource resource);

	void aliasingBarrier(const ref<dx_texture>& before, const ref<dx_texture>& after);
	void aliasingBarrier(const ref<dx_buffer>& before, const ref<dx_buffer>& after);
	void aliasingBarrier(dx_resource before, dx_resource after);


	// Copy.
	void copyResource(dx_resource from, dx_resource to);
	void copyTextureRegionToBuffer(const ref<dx_texture>& from, const ref<dx_buffer>& to, uint32 x, uint32 y, uint32 width, uint32 height);


	// Pipeline.
	void setPipelineState(dx_pipeline_state pipelineState);
	void setPipelineState(dx_raytracing_pipeline_state pipelineState);


	// Uniforms.
	void setGraphicsRootSignature(const dx_root_signature& rootSignature);
	void setGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T> void setGraphics32BitConstants(uint32 rootParameterIndex, const T& constants);

	void setComputeRootSignature(const dx_root_signature& rootSignature);
	void setCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants);
	template<typename T> void setCompute32BitConstants(uint32 rootParameterIndex, const T& constants);

	dx_allocation allocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	dx_dynamic_constant_buffer uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data);
	template <typename T> dx_dynamic_constant_buffer uploadDynamicConstantBuffer(const T& data);

	dx_dynamic_constant_buffer uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data);
	template <typename T> dx_dynamic_constant_buffer uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, const T& data);

	void setGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address);

	dx_dynamic_constant_buffer uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data);
	template <typename T> dx_dynamic_constant_buffer uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, const T& data);

	void setComputeDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address);

	dx_dynamic_vertex_buffer createDynamicVertexBuffer(uint32 elementSize, uint32 elementCount, void* data);

	void setRootGraphicsUAV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer);
	void setRootGraphicsUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	void setRootComputeUAV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer);
	void setRootComputeUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	
	void setRootGraphicsSRV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer);
	void setRootGraphicsSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);
	void setRootComputeSRV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer);
	void setRootComputeSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address);

	void setDescriptorHeapResource(uint32 rootParameterIndex, uint32 offset, uint32 count, dx_cpu_descriptor_handle handle);
	void setDescriptorHeapSRV(uint32 rootParameterIndex, uint32 offset, dx_cpu_descriptor_handle handle) { setDescriptorHeapResource(rootParameterIndex, offset, 1, handle); }
	void setDescriptorHeapSRV(uint32 rootParameterIndex, uint32 offset, const ref<dx_texture>& texture) { setDescriptorHeapResource(rootParameterIndex, offset, 1, texture->defaultSRV); }
	void setDescriptorHeapSRV(uint32 rootParameterIndex, uint32 offset, const ref<dx_buffer>& buffer) { setDescriptorHeapResource(rootParameterIndex, offset, 1, buffer->defaultSRV); }
	void setDescriptorHeapUAV(uint32 rootParameterIndex, uint32 offset, dx_cpu_descriptor_handle handle) { setDescriptorHeapResource(rootParameterIndex, offset, 1, handle); }
	void setDescriptorHeapUAV(uint32 rootParameterIndex, uint32 offset, const ref<dx_texture>& texture) { setDescriptorHeapResource(rootParameterIndex, offset, 1, texture->defaultUAV); }
	void setDescriptorHeapUAV(uint32 rootParameterIndex, uint32 offset, const ref<dx_buffer>& buffer) { setDescriptorHeapResource(rootParameterIndex, offset, 1, buffer->defaultUAV); }


	// Shader resources.
	template <typename T> void setDescriptorHeap(dx_descriptor_heap<T>& descriptorHeap);
	void setDescriptorHeap(dx_descriptor_range& descriptorRange);
	void setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, com<ID3D12DescriptorHeap> descriptorHeap);
	void resetToDynamicDescriptorHeap();
	void setGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void setComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);

	void clearUAV(const ref<dx_buffer>& buffer, float val = 0.f);
	void clearUAV(dx_resource resource, dx_cpu_descriptor_handle cpuHandle, dx_gpu_descriptor_handle gpuHandle, float val = 0.f);
	void clearUAV(dx_resource resource, dx_cpu_descriptor_handle cpuHandle, dx_gpu_descriptor_handle gpuHandle, uint32 val = 0);


	// Input assembly.
	void setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void setVertexBuffer(uint32 slot, const ref<dx_vertex_buffer>& buffer);
	void setVertexBuffer(uint32 slot, dx_dynamic_vertex_buffer buffer);
	void setVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer);
	void setIndexBuffer(const ref<dx_index_buffer>& buffer);
	void setIndexBuffer(D3D12_INDEX_BUFFER_VIEW& buffer);


	// Rasterizer.
	void setViewport(const D3D12_VIEWPORT& viewport);
	void setScissor(const D3D12_RECT& scissor);


	// Render targets.
	void setRenderTarget(dx_rtv_descriptor_handle* rtvs, uint32 numRTVs, dx_dsv_descriptor_handle* dsv);
	void setRenderTarget(dx_render_target& renderTarget);

	void clearRTV(dx_rtv_descriptor_handle rtv, float r, float g, float b, float a = 1.f);
	void clearRTV(dx_rtv_descriptor_handle rtv, const float* clearColor);
	void clearRTV(const ref<dx_texture>& texture, float r, float g, float b, float a = 1.f);
	void clearRTV(const ref<dx_texture>& texture, const float* clearColor);
	void clearRTV(dx_render_target& renderTarget, uint32 attachment, const float* clearColor);
	void clearRTV(dx_render_target& renderTarget, uint32 attachment, float r, float g, float b, float a = 1.f);

	void clearDepth(dx_dsv_descriptor_handle dsv, float depth = 1.f);
	void clearDepth(dx_render_target& renderTarget, float depth = 1.f);

	void clearStencil(dx_dsv_descriptor_handle dsv, uint32 stencil = 0);
	void clearStencil(dx_render_target& renderTarget, uint32 stencil = 0);

	void clearDepthAndStencil(dx_dsv_descriptor_handle dsv, float depth = 1.f, uint32 stencil = 0);
	void clearDepthAndStencil(dx_render_target& renderTarget, float depth = 1.f, uint32 stencil = 0);

	void setStencilReference(uint32 stencilReference);
	void setBlendFactor(const float* blendFactor);


	// Draw.
	void draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);
	void drawIndirect(dx_command_signature commandSignature, uint32 numDraws, const ref<dx_buffer>& commandBuffer);
	void drawIndirect(dx_command_signature commandSignature, uint32 maxNumDraws, const ref<dx_buffer>& numDrawsBuffer, const ref<dx_buffer>& commandBuffer);
	void drawFullscreenTriangle();


	// Dispatch.
	void dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);
	void dispatchIndirect(dx_command_signature commandSignature, uint32 numDispatches, const ref<dx_buffer>& commandBuffer);
	void dispatchIndirect(dx_command_signature commandSignature, uint32 maxNumDispatches, const ref<dx_buffer>& numDispatchesBuffer, const ref<dx_buffer>& commandBuffer);


	// Raytracing.
	void raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc);

	void reset();
};

template<typename T>
void dx_command_list::setDescriptorHeap(dx_descriptor_heap<T>& descriptorHeap)
{
	descriptorHeaps[descriptorHeap.type] = descriptorHeap.descriptorHeap.Get();

	uint32 numDescriptorHeaps = 0;
	ID3D12DescriptorHeap* heaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* heap = descriptorHeaps[i];
		if (heap)
		{
			heaps[numDescriptorHeaps++] = heap;
		}
	}

	commandList->SetDescriptorHeaps(numDescriptorHeaps, heaps);
}

template<typename T>
void dx_command_list::setGraphics32BitConstants(uint32 rootParameterIndex, const T& constants)
{
	static_assert(sizeof(T) % 4 == 0, "Size of type must be a multiple of 4 bytes.");
	setGraphics32BitConstants(rootParameterIndex, sizeof(T) / 4, &constants);
}

template<typename T>
void dx_command_list::setCompute32BitConstants(uint32 rootParameterIndex, const T& constants)
{
	static_assert(sizeof(T) % 4 == 0, "Size of type must be a multiple of 4 bytes.");
	setCompute32BitConstants(rootParameterIndex, sizeof(T) / 4, &constants);
}

template <typename T> dx_dynamic_constant_buffer dx_command_list::uploadDynamicConstantBuffer(const T& data)
{
	return uploadDynamicConstantBuffer(sizeof(T), &data);
}

template <typename T> dx_dynamic_constant_buffer dx_command_list::uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, const T& data)
{
	return uploadAndSetGraphicsDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
}

template <typename T> dx_dynamic_constant_buffer dx_command_list::uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, const T& data)
{
	return uploadAndSetComputeDynamicConstantBuffer(rootParameterIndex, sizeof(T), &data);
}
