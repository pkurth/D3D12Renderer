#include "pch.h"
#include "dx_command_list.h"

void dx_command_list::barriers(CD3DX12_RESOURCE_BARRIER* barriers, uint32 numBarriers)
{
	commandList->ResourceBarrier(numBarriers, barriers);
}

void dx_command_list::transitionBarrier(dx_texture& texture, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	transitionBarrier(texture.resource, from, to, subresource);
}

void dx_command_list::transitionBarrier(dx_buffer& buffer, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	transitionBarrier(buffer.resource, from, to, subresource);
}

void dx_command_list::transitionBarrier(dx_resource resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), from, to, subresource);
	commandList->ResourceBarrier(1, &barrier);
}

void dx_command_list::uavBarrier(dx_texture& texture)
{
	uavBarrier(texture.resource);
}

void dx_command_list::uavBarrier(dx_buffer& buffer)
{
	uavBarrier(buffer.resource);
}

void dx_command_list::uavBarrier(dx_resource resource)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	commandList->ResourceBarrier(1, &barrier);
}

void dx_command_list::aliasingBarrier(dx_texture& before, dx_texture& after)
{
	aliasingBarrier(before.resource, after.resource);
}

void dx_command_list::aliasingBarrier(dx_buffer& before, dx_buffer& after)
{
	aliasingBarrier(before.resource, after.resource);
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

void dx_command_list::setGraphicsRootSignature(dx_root_signature rootSignature)
{
	commandList->SetGraphicsRootSignature(rootSignature.Get());
}

void dx_command_list::setGraphics32BitConstants(uint32 rootParameterIndex, uint32 numConstants, const void* constants)
{
	commandList->SetGraphicsRoot32BitConstants(rootParameterIndex, numConstants, constants, 0);
}

void dx_command_list::setComputeRootSignature(dx_root_signature rootSignature)
{
	commandList->SetComputeRootSignature(rootSignature.Get());
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

void dx_command_list::setGraphicsDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle)
{
	commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, handle);
}

void dx_command_list::setGraphicsDescriptorTable(uint32 rootParameterIndex, dx_descriptor_handle handle)
{
	setGraphicsDescriptorTable(rootParameterIndex, handle.gpuHandle);
}

void dx_command_list::setComputeDescriptorTable(uint32 rootParameterIndex, CD3DX12_GPU_DESCRIPTOR_HANDLE handle)
{
	commandList->SetComputeRootDescriptorTable(rootParameterIndex, handle);
}

void dx_command_list::setComputeDescriptorTable(uint32 rootParameterIndex, dx_descriptor_handle handle)
{
	setComputeDescriptorTable(rootParameterIndex, handle.gpuHandle);
}

void dx_command_list::setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY topology)
{
	commandList->IASetPrimitiveTopology(topology);
}

void dx_command_list::setVertexBuffer(uint32 slot, dx_vertex_buffer& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer.view);
}

void dx_command_list::setVertexBuffer(uint32 slot, dx_dynamic_vertex_buffer buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer.view);
}

void dx_command_list::setVertexBuffer(uint32 slot, D3D12_VERTEX_BUFFER_VIEW& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer);
}

void dx_command_list::setIndexBuffer(dx_index_buffer& buffer)
{
	commandList->IASetIndexBuffer(&buffer.view);
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

void dx_command_list::setScreenRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE* rtvs, uint32 numRTVs, D3D12_CPU_DESCRIPTOR_HANDLE* dsv)
{
	commandList->OMSetRenderTargets(numRTVs, rtvs, FALSE, dsv);
}

void dx_command_list::setRenderTarget(dx_render_target& renderTarget, uint32 arraySlice)
{
	D3D12_CPU_DESCRIPTOR_HANDLE* dsv = (renderTarget.depthStencilFormat != DXGI_FORMAT_UNKNOWN) ? &renderTarget.dsvHandle : 0;
	commandList->OMSetRenderTargets(renderTarget.numAttachments, renderTarget.rtvHandles, FALSE, dsv);
}

void dx_command_list::clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, float r, float g, float b, float a)
{
	float clearColor[] = { r, g, b, a };
	clearRTV(rtv, clearColor);
}

void dx_command_list::clearRTV(D3D12_CPU_DESCRIPTOR_HANDLE rtv, const float* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, 0);
}

void dx_command_list::clearDepth(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, 0);
}

void dx_command_list::clearStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32 stencil)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.f, stencil, 0, 0);
}

void dx_command_list::clearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth, uint32 stencil)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, 0);
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
	commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
}

void dx_command_list::drawIndexed(uint32 indexCount, uint32 instanceCount, uint32 startIndex, int32 baseVertex, uint32 startInstance)
{
	commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 numDraws, dx_buffer commandBuffer)
{
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		numDraws,
		commandBuffer.resource.Get(),
		0,
		0,
		0);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 maxNumDraws, dx_buffer numDrawsBuffer, dx_buffer commandBuffer)
{
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDraws,
		commandBuffer.resource.Get(),
		0,
		numDrawsBuffer.resource.Get(),
		0);
}

void dx_command_list::drawFullscreenTriangle()
{
	setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	draw(3, 1, 0, 0);
}

void dx_command_list::dispatch(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ)
{
	commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void dx_command_list::dispatchIndirect(dx_command_signature commandSignature, uint32 numDispatches, dx_buffer commandBuffer)
{
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		numDispatches,
		commandBuffer.resource.Get(),
		0,
		0,
		0);
}

void dx_command_list::dispatchIndirect(dx_command_signature commandSignature, uint32 maxNumDispatches, dx_buffer numDispatchesBuffer, dx_buffer commandBuffer)
{
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumDispatches,
		commandBuffer.resource.Get(),
		0,
		numDispatchesBuffer.resource.Get(),
		0);
}

void dx_command_list::raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc)
{
	commandList->DispatchRays(&raytraceDesc);
}

void dx_command_list::reset(dx_command_allocator* commandAllocator)
{
	this->commandAllocator = commandAllocator;
	checkResult(commandList->Reset(commandAllocator->commandAllocator.Get(), 0));

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		descriptorHeaps[i] = 0;
	}

	uploadBuffer.reset();
}
