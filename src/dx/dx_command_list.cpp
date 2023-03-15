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

void dx_command_list::assertResourceState(const ref<dx_texture>& texture, D3D12_RESOURCE_STATES state, uint32 subresource)
{
	assertResourceState(texture->resource, state, subresource);
}

void dx_command_list::assertResourceState(const ref<dx_buffer>& buffer, D3D12_RESOURCE_STATES state, uint32 subresource)
{
	assertResourceState(buffer->resource, state, subresource);
}

void dx_command_list::assertResourceState(dx_resource resource, D3D12_RESOURCE_STATES state, uint32 subresource)
{
#ifdef _DEBUG
	ID3D12DebugCommandList* debugCL = 0;
	if (SUCCEEDED(commandList->QueryInterface(IID_PPV_ARGS(&debugCL))))
	{
		ASSERT(debugCL->AssertResourceState(resource.Get(), subresource, state));
	}
#endif
}

void dx_command_list::copyResource(dx_resource from, dx_resource to)
{
	commandList->CopyResource(to.Get(), from.Get());
}

void dx_command_list::copyTextureRegionToBuffer(const ref<dx_texture>& from, const ref<dx_buffer>& to, uint32 bufferElementOffset, uint32 x, uint32 y, uint32 width, uint32 height)
{
	copyTextureRegionToBuffer(from->resource, from->width, from->format, to, bufferElementOffset, x, y, width, height);
}

void dx_command_list::copyTextureRegionToBuffer(const dx_resource& from, uint32 fromWidth, DXGI_FORMAT format, const ref<dx_buffer>& to, uint32 bufferElementOffset, uint32 x, uint32 y, uint32 width, uint32 height)
{
	uint32 numPixelsToCopy = width * height;

	uint32 numRows = bucketize(numPixelsToCopy, fromWidth);
	uint32 destWidth = (numRows == 1) ? to->elementCount : fromWidth;

	D3D12_TEXTURE_COPY_LOCATION destLocation = {};
	destLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destLocation.pResource = to->resource.Get();
	destLocation.SubresourceIndex = 0;
	destLocation.PlacedFootprint.Offset = 0;
	destLocation.PlacedFootprint.Footprint.Format = format;
	destLocation.PlacedFootprint.Footprint.Width = destWidth;
	destLocation.PlacedFootprint.Footprint.Height = numRows;
	destLocation.PlacedFootprint.Footprint.Depth = 1;
	destLocation.PlacedFootprint.Footprint.RowPitch = alignTo(destWidth * getFormatSize(format), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLocation.SubresourceIndex = 0;
	srcLocation.pResource = from.Get();

	D3D12_BOX srcBox = { x, y, 0, x + width, y + height, 1 };
	commandList->CopyTextureRegion(&destLocation, bufferElementOffset, 0, 0, &srcLocation, &srcBox);
}

void dx_command_list::copyTextureRegionToTexture(const ref<dx_texture>& from, const ref<dx_texture>& to, uint32 sourceX, uint32 sourceY, uint32 destX, uint32 destY, uint32 width, uint32 height)
{
	D3D12_TEXTURE_COPY_LOCATION destLocation = { to->resource.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };
	D3D12_TEXTURE_COPY_LOCATION srcLocation = { from->resource.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, 0 };

	D3D12_BOX srcBox = { sourceX, sourceY, 0, sourceX + width, sourceY + height, 1 };
	commandList->CopyTextureRegion(&destLocation, destX, destY, 0, &srcLocation, &srcBox);
}

void dx_command_list::copyBufferRegionToBuffer(const ref<dx_buffer>& from, const ref<dx_buffer>& to, uint32 fromElementOffset, uint32 numElements, uint32 toElementOffset)
{
	commandList->CopyBufferRegion(to->resource.Get(), toElementOffset * to->elementSize, from->resource.Get(), fromElementOffset * from->elementSize, numElements * from->elementSize);
}

void dx_command_list::copyBufferRegionToBuffer_ByteOffset(const ref<dx_buffer>& from, const ref<dx_buffer>& to, uint32 fromByteOffset, uint32 numBytes, uint32 toByteOffset)
{
	commandList->CopyBufferRegion(to->resource.Get(), toByteOffset, from->resource.Get(), fromByteOffset, numBytes);
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

void dx_command_list::setGraphicsDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address)
{
	commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, address.gpuPtr);
}

void dx_command_list::setComputeDynamicConstantBuffer(uint32 rootParameterIndex, dx_dynamic_constant_buffer address)
{
	commandList->SetComputeRootConstantBufferView(rootParameterIndex, address.gpuPtr);
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

void dx_command_list::clearUAV(const ref<dx_buffer>& buffer, uint32 val)
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

void dx_command_list::setVertexBuffer(uint32 slot, const dx_dynamic_vertex_buffer& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer.view);
}

void dx_command_list::setVertexBuffer(uint32 slot, const D3D12_VERTEX_BUFFER_VIEW& buffer)
{
	commandList->IASetVertexBuffers(slot, 1, &buffer);
}

void dx_command_list::setIndexBuffer(const ref<dx_index_buffer>& buffer)
{
	commandList->IASetIndexBuffer(&buffer->view);
}

void dx_command_list::setIndexBuffer(const dx_dynamic_index_buffer& buffer)
{
	commandList->IASetIndexBuffer(&buffer.view);
}

void dx_command_list::setIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& buffer)
{
	commandList->IASetIndexBuffer(&buffer);
}

void dx_command_list::setViewport(const D3D12_VIEWPORT& viewport)
{
	commandList->RSSetViewports(1, &viewport);
}

void dx_command_list::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	D3D12_VIEWPORT viewport = { x, y, width, height, minDepth, maxDepth };
	setViewport(viewport);
}

void dx_command_list::setScissor(const D3D12_RECT& scissor)
{
	commandList->RSSetScissorRects(1, &scissor);
}

void dx_command_list::setRenderTarget(const dx_rtv_descriptor_handle* rtvs, uint32 numRTVs, const dx_dsv_descriptor_handle* dsv)
{
	commandList->OMSetRenderTargets(numRTVs, (D3D12_CPU_DESCRIPTOR_HANDLE*)rtvs, FALSE, (D3D12_CPU_DESCRIPTOR_HANDLE*)dsv);
}

void dx_command_list::setRenderTarget(const dx_render_target& renderTarget)
{
	setRenderTarget(renderTarget.rtv, renderTarget.numAttachments, renderTarget.dsv ? &renderTarget.dsv : 0);
}

static void getD3D12Rects(D3D12_RECT* d3rects, const clear_rect* rects, uint32 numRects)
{
	for (uint32 i = 0; i < numRects; ++i)
	{
		d3rects[i] = { (LONG)rects[i].x, (LONG)rects[i].y, (LONG)rects[i].x + (LONG)rects[i].width, (LONG)rects[i].y + (LONG)rects[i].height };
	}
}

void dx_command_list::clearRTV(dx_rtv_descriptor_handle rtv, float r, float g, float b, float a, const clear_rect* rects, uint32 numRects)
{
	float clearColor[] = { r, g, b, a };
	clearRTV(rtv, clearColor, rects, numRects);
}

void dx_command_list::clearRTV(dx_rtv_descriptor_handle rtv, const float* clearColor, const clear_rect* rects, uint32 numRects)
{
	D3D12_RECT* d3rects = 0;
	if (numRects)
	{
		d3rects = (D3D12_RECT*)alloca(sizeof(D3D12_RECT) * numRects);
		getD3D12Rects(d3rects, rects, numRects);
	}

	commandList->ClearRenderTargetView(rtv, clearColor, numRects, d3rects);
}

void dx_command_list::clearRTV(const ref<dx_texture>& texture, float r, float g, float b, float a, const clear_rect* rects, uint32 numRects)
{
	clearRTV(texture->defaultRTV, r, g, b, a, rects, numRects);
}

void dx_command_list::clearRTV(const ref<dx_texture>& texture, const float* clearColor, const clear_rect* rects, uint32 numRects)
{
	clearRTV(texture->defaultRTV, clearColor, rects, numRects);
}

void dx_command_list::clearRTV(const dx_render_target& renderTarget, uint32 attachment, const float* clearColor, const clear_rect* rects, uint32 numRects)
{
	clearRTV(renderTarget.rtv[attachment], clearColor, rects, numRects);
}

void dx_command_list::clearRTV(const dx_render_target& renderTarget, uint32 attachment, float r, float g, float b, float a, const clear_rect* rects, uint32 numRects)
{
	clearRTV(renderTarget.rtv[attachment], r, g, b, a, rects, numRects);
}

void dx_command_list::clearDepth(dx_dsv_descriptor_handle dsv, float depth, const clear_rect* rects, uint32 numRects)
{
	D3D12_RECT* d3rects = 0;
	if (numRects)
	{
		d3rects = (D3D12_RECT*)alloca(sizeof(D3D12_RECT) * numRects);
		getD3D12Rects(d3rects, rects, numRects);
	}

	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, numRects, d3rects);
}

void dx_command_list::clearDepth(const ref<dx_texture>& texture, float depth, const clear_rect* rects, uint32 numRects)
{
	clearDepth(texture->defaultDSV, depth, rects, numRects);
}

void dx_command_list::clearDepth(const dx_render_target& renderTarget, float depth, const clear_rect* rects, uint32 numRects)
{
	clearDepth(renderTarget.dsv, depth, rects, numRects);
}

void dx_command_list::clearStencil(dx_dsv_descriptor_handle dsv, uint32 stencil, const clear_rect* rects, uint32 numRects)
{
	D3D12_RECT* d3rects = 0;
	if (numRects)
	{
		d3rects = (D3D12_RECT*)alloca(sizeof(D3D12_RECT) * numRects);
		getD3D12Rects(d3rects, rects, numRects);
	}

	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_STENCIL, 0.f, stencil, numRects, d3rects);
}

void dx_command_list::clearStencil(const ref<dx_texture>& texture, uint32 stencil, const clear_rect* rects, uint32 numRects)
{
	clearStencil(texture->defaultDSV, stencil, rects, numRects);
}

void dx_command_list::clearStencil(const dx_render_target& renderTarget, uint32 stencil, const clear_rect* rects, uint32 numRects)
{
	clearStencil(renderTarget.dsv, stencil, rects, numRects);
}

void dx_command_list::clearDepthAndStencil(dx_dsv_descriptor_handle dsv, float depth, uint32 stencil, const clear_rect* rects, uint32 numRects)
{
	D3D12_RECT* d3rects = 0;
	if (numRects)
	{
		d3rects = (D3D12_RECT*)alloca(sizeof(D3D12_RECT) * numRects);
		getD3D12Rects(d3rects, rects, numRects);
	}

	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, numRects, d3rects);
}

void dx_command_list::clearDepthAndStencil(const ref<dx_texture>& texture, float depth, uint32 stencil, const clear_rect* rects, uint32 numRects)
{
	clearDepthAndStencil(texture->defaultDSV, depth, stencil, rects, numRects);
}

void dx_command_list::clearDepthAndStencil(const dx_render_target& renderTarget, float depth, uint32 stencil, const clear_rect* rects, uint32 numRects)
{
	clearDepthAndStencil(renderTarget.dsv, depth, stencil, rects, numRects);
}

void dx_command_list::setStencilReference(uint32 stencilReference)
{
	commandList->OMSetStencilRef(stencilReference);
}

void dx_command_list::setBlendFactor(float blendR, float blendG, float blendB, float blendA)
{
	const float blendFactor[] = { blendR, blendG, blendB, blendA };
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

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 numCommands, const dx_resource& commandBuffer, uint32 commandBufferOffset)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		numCommands,
		commandBuffer.Get(),
		commandBufferOffset,
		0,
		0);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 maxNumCommands, const dx_resource& numDrawsBuffer, const dx_resource& commandBuffer, uint32 commandBufferOffset)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumCommands,
		commandBuffer.Get(),
		commandBufferOffset,
		numDrawsBuffer.Get(),
		0);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 numCommands, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset)
{
	drawIndirect(commandSignature, numCommands, commandBuffer->resource, commandBufferOffset);
}

void dx_command_list::drawIndirect(dx_command_signature commandSignature, uint32 maxNumCommands, const ref<dx_buffer>& numDrawsBuffer, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset)
{
	drawIndirect(commandSignature, maxNumCommands, numDrawsBuffer->resource, commandBuffer->resource, commandBufferOffset);
}

void dx_command_list::drawFullscreenTriangle()
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	draw(3, 1, 0, 0);
}

void dx_command_list::drawCubeTriangleStrip()
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	draw(14, 1, 0, 0);
}

void dx_command_list::dispatch(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
}

void dx_command_list::dispatchIndirect(dx_command_signature commandSignature, uint32 numCommands, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		numCommands,
		commandBuffer->resource.Get(),
		commandBufferOffset,
		0,
		0);
}

void dx_command_list::dispatchIndirect(dx_command_signature commandSignature, uint32 maxNumCommands, const ref<dx_buffer>& numDispatchesBuffer, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->ExecuteIndirect(
		commandSignature.Get(),
		maxNumCommands,
		commandBuffer->resource.Get(),
		commandBufferOffset,
		numDispatchesBuffer->resource.Get(),
		0);
}

void dx_command_list::dispatchIndirect(uint32 numCommands, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset)
{
	dispatchIndirect(dxContext.defaultDispatchCommandSignature, numCommands, commandBuffer, commandBufferOffset);
}

void dx_command_list::dispatchIndirect(uint32 maxNumCommands, const ref<dx_buffer>& numDispatchesBuffer, const ref<dx_buffer>& commandBuffer, uint32 commandBufferOffset)
{
	dispatchIndirect(dxContext.defaultDispatchCommandSignature, maxNumCommands, numDispatchesBuffer, commandBuffer, commandBufferOffset);
}

void dx_command_list::dispatchMesh(uint32 numGroupsX, uint32 numGroupsY, uint32 numGroupsZ)
{
#ifdef SDK_SUPPORTS_MESH_SHADERS
	dynamicDescriptorHeap.commitStagedDescriptorsForDraw(this);
	commandList->DispatchMesh(numGroupsX, numGroupsY, numGroupsZ);
#else
	ASSERT(!"Mesh shaders are not supported with your Windows SDK version.");
#endif
}

void dx_command_list::raytrace(D3D12_DISPATCH_RAYS_DESC& raytraceDesc)
{
	dynamicDescriptorHeap.commitStagedDescriptorsForDispatch(this);
	commandList->DispatchRays(&raytraceDesc);
}

void dx_command_list::queryTimestamp(uint32 index)
{
	commandList->EndQuery(timeStampQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
}

void dx_command_list::reset()
{
	commandAllocator->Reset();
	checkResult(commandList->Reset(commandAllocator.Get(), 0));

	for (uint32 i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		descriptorHeaps[i] = 0;
	}

	dynamicDescriptorHeap.reset();
}
