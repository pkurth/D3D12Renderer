#pragma once

#include "dx.h"
#include "dx_upload_buffer.h"
#include "dx_dynamic_descriptor_heap.h"
#include "dx_descriptor_allocation.h"
#include "dx_texture.h"
#include "dx_buffer.h"
#include "dx_render_target.h"
#include "dx_pipeline.h"


struct clear_rect
{
	uint32 x, y, width, height;
};

struct dx_command_list
{
	dx_command_list(D3D12_COMMAND_LIST_TYPE type);

	D3D12_COMMAND_LIST_TYPE type;
	dx_command_allocator commandAllocator;
	dx_graphics_command_list commandList;
	uint64 lastExecutionFenceValue;
	dx_command_list* next;

	dx_dynamic_descriptor_heap dynamicDescriptorHeap;

	dx_query_heap timeStampQueryHeap;


	ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};


	// Barriers.
	void barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers);

	// Avoid calling these. Instead, use the dx_barrier_batcher interface to batch multiple barriers into a single submission.
	void transitionBarrier(const ref<dx_texture>& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void transitionBarrier(const ref<dx_buffer>& buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void transitionBarrier(dx_resource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void uavBarrier(const ref<dx_texture>& texture);
	void uavBarrier(const ref<dx_buffer>& buffer);
	void uavBarrier(dx_resource resource);

	void aliasingBarrier(const ref<dx_texture>& before, const ref<dx_texture>& after);
	void aliasingBarrier(const ref<dx_buffer>& before, const ref<dx_buffer>& after);
	void aliasingBarrier(dx_resource before, dx_resource after);

	void assertResourceState(const ref<dx_texture>& texture, D3D12_RESOURCE_STATES state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void assertResourceState(const ref<dx_buffer>& buffer, D3D12_RESOURCE_STATES state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void assertResourceState(dx_resource resource, D3D12_RESOURCE_STATES state, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);


	// Copy.
	void copyResource(dx_resource from, dx_resource to);
	void copyTextureRegionToBuffer(const ref<dx_texture>& from, const ref<dx_buffer>& to, uint32 bufferElementOffset, uint32 x, uint32 y, uint32 width, uint32 height);
	void copyTextureRegionToBuffer(const dx_resource& from, uint32 fromWidth, DXGI_FORMAT format, const ref<dx_buffer>& to, uint32 bufferElementOffset, uint32 x, uint32 y, uint32 width, uint32 height);
	void copyTextureRegionToTexture(const ref<dx_texture>& from, const ref<dx_texture>& to, uint32 sourceX, uint32 sourceY, uint32 destX, uint32 destY, uint32 width, uint32 height);
	void copyBufferRegionToBuffer(const ref<dx_buffer>& from, const ref<dx_buffer>& to, uint32 fromElementOffset, uint32 numElements, uint32 toElementOffset);
	void copyBufferRegionToBuffer_ByteOffset(const ref<dx_buffer>& from, const ref<dx_buffer>& to, uint32 fromByteOffset, uint32 numBytes, uint32 toByteOffset);


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

	void setGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address);
	void setComputeDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address);

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
	void setDescriptorHeap(dx_descriptor_range& descriptorRange);
	void setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, com<ID3D12DescriptorHeap> descriptorHeap);
	void resetToDynamicDescriptorHeap();
	void setGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);
	void setComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle);

	void clearUAV(const ref<dx_buffer>& buffer, float val = 0.f);
	void clearUAV(const ref<dx_buffer>& buffer, uint32 val = 0);
	void clearUAV(dx_resource resource, dx_cpu_descriptor_handle cpuHandle, dx_gpu_descriptor_handle gpuHandle, float val = 0.f);
	void clearUAV(dx_resource resource, dx_cpu_descriptor_handle cpuHandle, dx_gpu_descriptor_handle gpuHandle, uint32 val = 0);


	// Input assembly.
	void setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology);
	void setVertexBuffer(uint32 slot, const ref<dx_vertex_buffer>& buffer);
	void setVertexBuffer(uint32 slot, const dx_dynamic_vertex_buffer& buffer);
	void setVertexBuffer(uint32 slot, const D3D12_VERTEX_BUFFER_VIEW& buffer);
	void setIndexBuffer(const ref<dx_index_buffer>& buffer);
	void setIndexBuffer(const dx_dynamic_index_buffer& buffer);
	void setIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& buffer);


	// Rasterizer.
	void setViewport(const D3D12_VIEWPORT& viewport);
	void setViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f);
	void setScissor(const D3D12_RECT& scissor);


	// Render targets.
	void setRenderTarget(const dx_rtv_descriptor_handle* rtvs, uint32 numRTVs, const dx_dsv_descriptor_handle* dsv);
	void setRenderTarget(const dx_render_target& renderTarget);

	void clearRTV(dx_rtv_descriptor_handle rtv, float r, float g, float b, float a = 1.f, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearRTV(dx_rtv_descriptor_handle rtv, const float* clearColor, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearRTV(const ref<dx_texture>& texture, float r, float g, float b, float a = 1.f, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearRTV(const ref<dx_texture>& texture, const float* clearColor, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearRTV(const dx_render_target& renderTarget, uint32 attachment, const float* clearColor, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearRTV(const dx_render_target& renderTarget, uint32 attachment, float r, float g, float b, float a = 1.f, const clear_rect* rects = 0, uint32 numRects = 0);

	void clearDepth(dx_dsv_descriptor_handle dsv, float depth = 1.f, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearDepth(const ref<dx_texture>& texture, float depth = 1.f, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearDepth(const dx_render_target& renderTarget, float depth = 1.f, const clear_rect* rects = 0, uint32 numRects = 0);

	void clearStencil(dx_dsv_descriptor_handle dsv, uint32 stencil = 0, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearStencil(const ref<dx_texture>& texture, uint32 stencil = 0, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearStencil(const dx_render_target& renderTarget, uint32 stencil = 0, const clear_rect* rects = 0, uint32 numRects = 0);

	void clearDepthAndStencil(dx_dsv_descriptor_handle dsv, float depth = 1.f, uint32 stencil = 0, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearDepthAndStencil(const ref<dx_texture>& texture, float depth = 1.f, uint32 stencil = 0, const clear_rect* rects = 0, uint32 numRects = 0);
	void clearDepthAndStencil(const dx_render_target& renderTarget, float depth = 1.f, uint32 stencil = 0, const clear_rect* rects = 0, uint32 numRects = 0);

	void setStencilReference(uint32 stencilReference);
	void setBlendFactor(float blendR, float blendG, float blendB, float blendA);


	// Draw.
	void draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance);
	void drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance);
	void drawIndirect(dx_command_signature commandSignature, uint32 numCommands, const dx_resource& commandBuffer, uint32 commandBufferOffset = 0);
	void drawIndirect(dx_command_signature commandSignature, uint32 maxNumCommands, const dx_resource& numDrawsBuffer, const dx_resource& commandBuffer, uint32 commandBufferOffset = 0);
	void drawIndirect(dx_command_signature commandSignature, uint32 numCommands, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset = 0);
	void drawIndirect(dx_command_signature commandSignature, uint32 maxNumCommands, const ref<dx_buffer>& numDrawsBuffer, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset = 0);
	void drawFullscreenTriangle();
	void drawCubeTriangleStrip();


	// Dispatch.
	void dispatch(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);
	void dispatchIndirect(dx_command_signature commandSignature, uint32 numCommands, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset = 0);
	void dispatchIndirect(dx_command_signature commandSignature, uint32 maxNumCommands, const ref<dx_buffer>& numDispatchesBuffer, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset = 0);
	void dispatchIndirect(uint32 numCommands, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset = 0);
	void dispatchIndirect(uint32 maxNumCommands, const ref<dx_buffer>& numDispatchesBuffer, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset = 0);


	// Mesh shaders.
	void dispatchMesh(uint32 numGroupsX, uint32 numGroupsY = 1, uint32 numGroupsZ = 1);


	// Raytracing.
	void raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc);


	// Queries.
	void queryTimestamp(uint32 index);

	void reset();
};

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
