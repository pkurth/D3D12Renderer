#include "pch.h"
#include "dx_command_list.h"
#include "dx_context.h"

dx_command_list::dx_command_list(D3D12_COMMAND_LIST_TYPE type)
{
	this->type = type;
	checkResult(dxContext.device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));
	checkResult(dxContext.device->CreateCommandList(0, type, commandAllocator.Get(), 0, IID_PPV_ARGS(&commandList)));

	dynamicDescriptorHeap.initialize();
}

void dx_command_list::barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers)
{
	commandList->ResourceBarrier(numBarriers, barriers);
}

void dx_command_list::transitionBarrier(const ref<dx_texture>& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	transitionBarrier(texture->resource, from, to, subresource);
}

void dx_command_list::transitionBarrier(const ref<dx_buffer>& buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	transitionBarrier(buffer->resource, from, to, subresource);
}

void dx_command_list::transitionBarrier(dx_resource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), from, to, subresource);
	commandList->ResourceBarrier(1, &barrier);
}

void dx_command_list::uavBarrier(const ref<dx_texture>& texture)
{
	uavBarrier(texture->resource);
}

void dx_command_list::uavBarrier(const ref<dx_buffer>& buffer)
{
	uavBarrier(buffer->resource);
}

void dx_command_list::uavBarrier(dx_resource resource)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	commandList->ResourceBarrier(1, &barrier);
}

void dx_command_list::aliasingBarrier(const ref<dx_texture>& before, const ref<dx_texture>& after)
{
	aliasingBarrier(before->resource, after->resource);
}

void dx_command_list::aliasingBarrier(const ref<dx_buffer>& before, const ref<dx_buffer>& after)
{
	aliasingBarrier(before->resource, after->resource);
}

void dx_command_list::aliasingBarrier(dx_resource before, dx_resource after)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	commandList->ResourceBarrier(1, &barrier);
}

void dx_command_list::copyResource(dx_resource from, dx_resource to)
{
	commandList->CopyResource(to.Get(), from.Get());
}

void dx_command_list::setPipelineState(dx_pipeline_state pipelineState)
{
	commandList->SetPipelineState(pipelineState.Get());
}

void dx_command_list::setPipelineState(dx_raytracing_pipeline_state pipelineState)
{
	commandList->SetPipelineState1(pipelineState.Get());
}

void dx_command_list::setGraphicsRootSignature(const dx_root_signature& rootSignature)
{
	dynamicDescriptorHeap.parseRootSignature(rootSignature);
	commandList->SetGraphicsRootSignature(rootSignature.rootSignature.Get());
}

void dx_command_list::setGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants)
{
	commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void dx_command_list::setComputeRootSignature(const dx_root_signature& rootSignature)
{
	dynamicDescriptorHeap.parseRootSignature(rootSignature);
	commandList->SetComputeRootSignature(rootSignature.rootSignature.Get());
}

void dx_command_list::setCompute32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants)
{
	commandList->SetComputeRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

dx_allocation dx_command_list::allocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment)
{
	dx_allocation allocation = uploadBuffer.allocate(sizeInBytes, alignment);
	return allocation;
}

dx_dynamic_constant_buffer dx_command_list::uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data)
{
	dx_allocation allocation = allocateDynamicBuffer(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(allocation.cpuPtr, data, sizeInBytes);
	return { allocation.gpuPtr, allocation.cpuPtr };
}

dx_dynamic_constant_buffer dx_command_list::uploadAndSetGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data)
{
	dx_dynamic_constant_buffer address = uploadDynamicConstantBuffer(sizeInBytes, data);
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.gpuPtr);
	return address;
}

void dx_command_list::setGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address)
{
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.gpuPtr);
}

dx_dynamic_constant_buffer dx_command_list::uploadAndSetComputeDynamicConstantBuffer(uint32 rootParameterIndex, uint32 sizeInBytes, const void* data)
{
	dx_dynamic_constant_buffer address = uploadDynamicConstantBuffer(sizeInBytes, data);
	commandList->SetComputeRootConstantBufferView(rootParameterIndex, address.gpuPtr);
	return address;
}

void dx_command_list::setComputeDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address)
{
	commandList->SetComputeRootConstantBufferView(rootParameterIndex, address.gpuPtr);
}

dx_dynamic_vertex_buffer dx_command_list::createDynamicVertexBuffer(uint32 elementSize, uint32 elementCount, void* data)
{
	uint32 sizeInBytes = elementSize * elementCount;
	dx_allocation allocation = allocateDynamicBuffer(sizeInBytes, elementSize);
	if (data)
	{
		memcpy(allocation.cpuPtr, data, sizeInBytes);
	}

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = allocation.gpuPtr;
	vertexBufferView.SizeInBytes = sizeInBytes;
	vertexBufferView.StrideInBytes = elementSize;

	return { vertexBufferView };
}

void dx_command_list::setRootGraphicsUAV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer)
{
	commandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, buffer->gpuVirtualAddress);
}

void dx_command_list::setRootGraphicsUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
{
	commandList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, address);
}

void dx_command_list::setRootComputeUAV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer)
{
	commandList->SetComputeRootUnorderedAccessView(rootParameterIndex, buffer->gpuVirtualAddress);
}

void dx_command_list::setRootComputeUAV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
{
	commandList->SetComputeRootUnorderedAccessView(rootParameterIndex, address);
}

void dx_command_list::setRootGraphicsSRV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer)
{
	commandList->SetGraphicsRootShaderResourceView(rootParameterIndex, buffer->gpuVirtualAddress);
}

void dx_command_list::setRootGraphicsSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
{
	commandList->SetGraphicsRootShaderResourceView(rootParameterIndex, address);
}

void dx_command_list::setRootComputeSRV(uint32 rootParameterIndex, const ref<dx_buffer>& buffer)
{
	commandList->SetComputeRootShaderResourceView(rootParameterIndex, buffer->gpuVirtualAddress);
}

void dx_command_list::setRootComputeSRV(uint32 rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS address)
{
	commandList->SetComputeRootShaderResourceView(rootParameterIndex, address);
}

void dx_command_list::setDescriptorHeapResource(uint32 rootParameterIndex, uint32 offset, uint32 count, dx_cpu_descriptor_handle handle)
{
	dynamicDescriptorHeap.stageDescriptors(rootParameterIndex, offset, count, handle);
}

void dx_command_list::setDescriptorHeap(dx_descriptor_heap& descriptorHeap)
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

void dx_command_list::setDescriptorHeap(dx_descriptor_range& descriptorRange)
{
	descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = descriptorRange.descriptorHeap.Get();

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

void dx_command_list::setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, com<ID3D12DescriptorHeap> descriptorHeap)
{
	descriptorHeaps[type] = descriptorHeap.Get();

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

void dx_command_list::resetToDynamicDescriptorHeap()
{
	dynamicDescriptorHeap.setCurrentDescriptorHeap(this);
}

void dx_command_list::setGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle)
{
	commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, handle);
}

void dx_command_list::setComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle)
{
	commandList->SetComputeRootDescriptorTable(rootParameterIndex, handle);
}

void dx_command_list::clearUAV(const ref<dx_buffer>& buffer, float val)
{
	clearUAV(buffer->resource, buffer->cpuClearUAV, buffer->gpuClearUAV, val);
}

void dx_command_list::clearUAV(dx_resource resource, dx_cpu_descriptor_handle cpuHandle, dx_gpu_descriptor_handle gpuHandle, float val)
{
	float vals[] = { val, val, val, val };
	commandList->ClearUnorderedAccessViewFloat(gpuHandle, cpuHandle, resource.Get(), vals, 0, 0);
}

void dx_command_list::clearUAV(dx_resource resource, dx_cpu_descriptor_handle cpuHandle, dx_gpu_descriptor_handle gpuHandle, uint32 val)
{
	uint32 vals[] = { val, val, val, val };
	commandList->ClearUnorderedAccessViewUint(gpuHandle, cpuHandle, resource.Get(), vals, 0, 0);
}

void dx_command_list::setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
{
	commandList->IASetPrimitiveTopology(topology);
}

void dx_command_list::setVertexBuffer(uint32 slot, const ref<dx_vertex_buffer>& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer->view);
}

void dx_command_list::setVertexBuffer(uint32 slot, dx_dynamic_vertex_buffer buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer.view);
}

void dx_command_list::setVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer);
}

void dx_command_list::setIndexBuffer(const ref<dx_index_buffer>& buffer)
{
	commandList->IASetIndexBuffer(&buffer->view);
}

void dx_command_list::setIndexBuffer(D3D12_INDEX_BUFFER_VIEW& buffer)
{
	commandList->IASetIndexBuffer(&buffer);
}

void dx_command_list::setViewport(const D3D12_VIEWPORT& viewport)
{
	commandList->RSSetViewports(1, &viewport);
}

void dx_command_list::setScissor(const D3D12_RECT& scissor)
{
	commandList->RSSetScissorRects(1, &scissor);
}

void dx_command_list::setRenderTarget(dx_cpu_descriptor_handle* rtvs, uint32 numRTVs, dx_cpu_descriptor_handle* dsv)
{
	commandList->OMSetRenderTargets(numRTVs, (D3D12_CPU_DESCRIPTOR_HANDLE*)rtvs, FALSE, (D3D12_CPU_DESCRIPTOR_HANDLE*)dsv);
}

void dx_command_list::setRenderTarget(dx_render_target& renderTarget)
{
	dx_cpu_descriptor_handle rtv[8];
	dx_cpu_descriptor_handle* dsv = 0;

	for (uint32 i = 0; i < renderTarget.numAttachments; ++i)
	{
		rtv[i] = renderTarget.colorAttachments[i]->rtvHandles;
	}
	if (renderTarget.depthAttachment)
	{
		dsv = &renderTarget.depthAttachment->dsvHandle;
	}

	setRenderTarget(rtv, renderTarget.numAttachments, dsv);
}

void dx_command_list::clearRTV(dx_cpu_descriptor_handle rtv, float r, float g, float b, float a)
{
	float clearColor[] = { r, g, b, a };
	clearRTV(rtv, clearColor);
}

void dx_command_list::clearRTV(dx_cpu_descriptor_handle rtv, const float* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, 0);
}

void dx_command_list::clearRTV(const ref<dx_texture>& texture, float r, float g, float b, float a)
{
	clearRTV(texture->rtvHandles, r, g, b, a);
}

void dx_command_list::clearRTV(const ref<dx_texture>& texture, const float* clearColor)
{
	clearRTV(texture->rtvHandles, clearColor);
}

void dx_command_list::clearRTV(dx_render_target& renderTarget, uint32 attachment, const float* clearColor)
{
	clearRTV(renderTarget.colorAttachments[attachment], clearColor);
}

void dx_command_list::clearRTV(dx_render_target& renderTarget, uint32 attachment, float r, float g, float b, float a)
{
	clearRTV(renderTarget.colorAttachments[attachment], r, g, b, a);
}

void dx_command_list::clearDepth(dx_cpu_descriptor_handle dsv, float depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, 0);
}

void dx_command_list::clearDepth(dx_render_target& renderTarget, float depth)
{
	clearDepth(renderTarget.depthAttachment->dsvHandle, depth);
}

void dx_command_list::clearStencil(dx_cpu_descriptor_handle dsv, uint32 stencil)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.f, stencil, 0, 0);
}

void dx_command_list::clearStencil(dx_render_target& renderTarget, uint32 stencil)
{
	clearStencil(renderTarget.depthAttachment->dsvHandle, stencil);
}

void dx_command_list::clearDepthAndStencil(dx_cpu_descriptor_handle dsv, float depth, uint32 stencil)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, 0);
}

void dx_command_list::clearDepthAndStencil(dx_render_target& renderTarget, float depth, uint32 stencil)
{
	clearDepthAndStencil(renderTarget.depthAttachment->dsvHandle, depth, stencil);
}

void dx_command_list::setStencilReference(uint32 stencilReference)
{
	commandList->OMSetStencilRef(stencilReference);
}

void dx_command_list::setBlendFactor(const float* blendFactor)
{
	commandList->OMSetBlendFactor(blendFactor);
}

void dx_command_list::draw(uint32 vertexCount, uint32 instanceCount, uint32 startVertex, uint32 startInstance)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void dx_command_list::drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 numDraws, const ref<dx_buffer>& commandBuffer)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		numDraws,
		commandBuffer->resource.Get(),
		0,
		0,
		0);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 maxNumDraws, const ref<dx_buffer>& numDrawsBuffer, const ref<dx_buffer>& commandBuffer)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDraws,
		commandBuffer->resource.Get(),
		0,
		numDrawsBuffer->resource.Get(),
		0);
}

void dx_command_list::drawFullscreenTriangle()
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	draw(3, 1, 0, 0);
}

void dx_command_list::dispatch(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void dx_command_list::dispatchIndirect(dx_command_signature commandSignature, uint32 numDispatches, const ref<dx_buffer>& commandBuffer)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		numDispatches,
		commandBuffer->resource.Get(),
		0,
		0,
		0);
}

void dx_command_list::dispatchIndirect(dx_command_signature commandSignature, uint32 maxNumDispatches, const ref<dx_buffer>& numDispatchesBuffer, const ref<dx_buffer>& commandBuffer)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDispatches,
		commandBuffer->resource.Get(),
		0,
		numDispatchesBuffer->resource.Get(),
		0);
}

void dx_command_list::raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->DispatchRays(&raytraceDesc);
}

void dx_command_list::reset()
{
	commandAllocator->Reset();
	checkResult(commandList->Reset(commandAllocator.Get(), 0));

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		descriptorHeaps[i] = 0;
	}

	uploadBuffer.reset();
	dynamicDescriptorHeap.reset();
}
