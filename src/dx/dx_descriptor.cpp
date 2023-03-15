#include "pch.h"
#include "dx_descriptor.h"
#include "dx_context.h"
#include "dx_texture.h"
#include "dx_buffer.h"

static uint32 getShader4ComponentMapping(DXGI_FORMAT format)
{
	switch (getNumberOfChannels(format))
	{
		case 1:
			return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
		case 2:
			return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
				D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0,
				D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
		case 3:
			return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
				D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
		case 4:
			return D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
	}
	return D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::create2DTextureSRV(const ref<dx_texture>& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = getShader4ComponentMapping(srvDesc.Format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = mipRange.first;
	srvDesc.Texture2D.MipLevels = mipRange.count;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createCubemapSRV(const ref<dx_texture>& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = getShader4ComponentMapping(srvDesc.Format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = mipRange.first;
	srvDesc.TextureCube.MipLevels = mipRange.count;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createCubemapArraySRV(const ref<dx_texture>& texture, texture_mip_range mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = getShader4ComponentMapping(srvDesc.Format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc.TextureCubeArray.MostDetailedMip = mipRange.first;
	srvDesc.TextureCubeArray.MipLevels = mipRange.count;
	srvDesc.TextureCubeArray.NumCubes = numCubes;
	srvDesc.TextureCubeArray.First2DArrayFace = firstCube * 6;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createVolumeTextureSRV(const ref<dx_texture>& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture->resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = getShader4ComponentMapping(srvDesc.Format);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
	srvDesc.Texture3D.MostDetailedMip = mipRange.first;
	srvDesc.Texture3D.MipLevels = mipRange.count;
	srvDesc.Texture3D.ResourceMinLODClamp = 0;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createDepthTextureSRV(const ref<dx_texture>& texture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = getDepthReadFormat(texture->format);
	srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createDepthTextureArraySRV(const ref<dx_texture>& texture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = getDepthReadFormat(texture->format);
	srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, 0);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = texture->resource->GetDesc().DepthOrArraySize;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createStencilTextureSRV(const ref<dx_texture>& texture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = getStencilReadFormat(texture->format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 1;

	dxContext.device->CreateShaderResourceView(texture->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createNullTextureSRV()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 0;

	dxContext.device->CreateShaderResourceView(0, &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createBufferSRV(const ref<dx_buffer>& buffer, buffer_range bufferRange)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = bufferRange.firstElement;
	srvDesc.Buffer.NumElements = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer->elementCount - bufferRange.firstElement);
	srvDesc.Buffer.StructureByteStride = buffer->elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dxContext.device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createRawBufferSRV(const ref<dx_buffer>& buffer, buffer_range bufferRange)
{
	uint32 firstElementByteOffset = bufferRange.firstElement * buffer->elementSize;
	ASSERT(firstElementByteOffset % 16 == 0);

	uint32 count = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer->elementCount - bufferRange.firstElement);
	uint32 totalSize = count * buffer->elementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = firstElementByteOffset / 4;
	srvDesc.Buffer.NumElements = totalSize / 4;
	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	dxContext.device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createNullBufferSRV()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = 1;
	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	dxContext.device->CreateShaderResourceView(0, &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::create2DTextureUAV(const ref<dx_texture>& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2D.MipSlice = mipSlice;
	dxContext.device->CreateUnorderedAccessView(texture->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::create2DTextureArrayUAV(const ref<dx_texture>& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = texture->resource->GetDesc().DepthOrArraySize;
	uavDesc.Texture2DArray.MipSlice = mipSlice;
	dxContext.device->CreateUnorderedAccessView(texture->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createCubemapUAV(const ref<dx_texture>& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = texture->resource->GetDesc().DepthOrArraySize;
	uavDesc.Texture2DArray.MipSlice = mipSlice;
	dxContext.device->CreateUnorderedAccessView(texture->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createVolumeTextureUAV(const ref<dx_texture>& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture3D.MipSlice = mipSlice;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = texture->depth;
	dxContext.device->CreateUnorderedAccessView(texture->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createNullTextureUAV()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	dxContext.device->CreateUnorderedAccessView(0, 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createBufferUAV(const ref<dx_buffer>& buffer, buffer_range bufferRange)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.FirstElement = bufferRange.firstElement;
	uavDesc.Buffer.NumElements = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer->elementCount - bufferRange.firstElement);
	uavDesc.Buffer.StructureByteStride = buffer->elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxContext.device->CreateUnorderedAccessView(buffer->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createBufferUintUAV(const ref<dx_buffer>& buffer, buffer_range bufferRange)
{
	uint32 firstElementByteOffset = bufferRange.firstElement * buffer->elementSize;
	ASSERT(firstElementByteOffset % 16 == 0);

	uint32 count = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer->elementCount - bufferRange.firstElement);
	uint32 totalSize = count * buffer->elementSize;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_UINT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.FirstElement = firstElementByteOffset / 4;
	uavDesc.Buffer.NumElements = totalSize / 4;
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxContext.device->CreateUnorderedAccessView(buffer->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createRawBufferUAV(const ref<dx_buffer>& buffer, buffer_range bufferRange)
{
	uint32 firstElementByteOffset = bufferRange.firstElement * buffer->elementSize;
	ASSERT(firstElementByteOffset % 16 == 0);

	uint32 count = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer->elementCount - bufferRange.firstElement);
	uint32 totalSize = count * buffer->elementSize;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = firstElementByteOffset / 4;
	uavDesc.Buffer.NumElements = totalSize / 4;
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	dxContext.device->CreateUnorderedAccessView(buffer->resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createNullBufferUAV()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;

	dxContext.device->CreateUnorderedAccessView(0, 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createRaytracingAccelerationStructureSRV(const ref<dx_buffer>& tlas)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlas->gpuVirtualAddress;

	dxContext.device->CreateShaderResourceView(nullptr, &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle dx_cpu_descriptor_handle::operator+(uint32 i)
{
	return { CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, i, dxContext.descriptorHandleIncrementSize) };
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::operator+=(uint32 i)
{
	cpuHandle.Offset(i, dxContext.descriptorHandleIncrementSize);
	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::operator++()
{
	cpuHandle.Offset(dxContext.descriptorHandleIncrementSize);
	return *this;
}

dx_cpu_descriptor_handle dx_cpu_descriptor_handle::operator++(int)
{
	dx_cpu_descriptor_handle result = *this;
	cpuHandle.Offset(dxContext.descriptorHandleIncrementSize);
	return result;
}

dx_gpu_descriptor_handle dx_gpu_descriptor_handle::operator+(uint32 i)
{
	return { CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, i, dxContext.descriptorHandleIncrementSize) };
}

dx_gpu_descriptor_handle& dx_gpu_descriptor_handle::operator+=(uint32 i)
{
	gpuHandle.Offset(i, dxContext.descriptorHandleIncrementSize);
	return *this;
}

dx_gpu_descriptor_handle& dx_gpu_descriptor_handle::operator++()
{
	gpuHandle.Offset(dxContext.descriptorHandleIncrementSize);
	return *this;
}

dx_gpu_descriptor_handle dx_gpu_descriptor_handle::operator++(int)
{
	dx_gpu_descriptor_handle result = *this;
	gpuHandle.Offset(dxContext.descriptorHandleIncrementSize);
	return result;
}

dx_rtv_descriptor_handle& dx_rtv_descriptor_handle::create2DTextureRTV(const ref<dx_texture>& texture, uint32 arraySlice, uint32 mipSlice)
{
	ASSERT(texture->supportsRTV);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texture->format;

	if (texture->depth == 1)
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = mipSlice;
		rtvDesc.Texture2D.PlaneSlice = 0;

		dxContext.device->CreateRenderTargetView(texture->resource.Get(), &rtvDesc, cpuHandle);
	}
	else
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.MipSlice = mipSlice;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		dxContext.device->CreateRenderTargetView(texture->resource.Get(), &rtvDesc, cpuHandle);
	}
	return *this;
}

dx_rtv_descriptor_handle& dx_rtv_descriptor_handle::createNullTextureRTV(DXGI_FORMAT format)
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	dxContext.device->CreateRenderTargetView(0, &rtvDesc, cpuHandle);

	return *this;
}

dx_dsv_descriptor_handle& dx_dsv_descriptor_handle::create2DTextureDSV(const ref<dx_texture>& texture, uint32 arraySlice, uint32 mipSlice)
{
	ASSERT(texture->supportsDSV);
	ASSERT(isDepthFormat(texture->format));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = texture->format;

	if (texture->depth == 1)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = mipSlice;

		dxContext.device->CreateDepthStencilView(texture->resource.Get(), &dsvDesc, cpuHandle);
	}
	else
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.FirstArraySlice = arraySlice;
		dsvDesc.Texture2DArray.ArraySize = 1;
		dsvDesc.Texture2DArray.MipSlice = mipSlice;

		dxContext.device->CreateDepthStencilView(texture->resource.Get(), &dsvDesc, cpuHandle);
	}
	return *this;
}
