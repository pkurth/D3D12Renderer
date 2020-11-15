#include "pch.h"
#include "dx_descriptor.h"

#include "dx_render_primitives.h"
#include "dx_context.h"

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::create2DTextureSRV(dx_texture& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture.resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = mipRange.first;
	srvDesc.Texture2D.MipLevels = mipRange.count;

	dxContext.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createCubemapSRV(dx_texture& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture.resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = mipRange.first;
	srvDesc.TextureCube.MipLevels = mipRange.count;

	dxContext.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createCubemapArraySRV(dx_texture& texture, texture_mip_range mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture.resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc.TextureCubeArray.MostDetailedMip = mipRange.first;
	srvDesc.TextureCubeArray.MipLevels = mipRange.count;
	srvDesc.TextureCubeArray.NumCubes = numCubes;
	srvDesc.TextureCubeArray.First2DArrayFace = firstCube * 6;

	dxContext.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createDepthTextureSRV(dx_texture& texture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = getReadFormatFromTypeless(texture.resource->GetDesc().Format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	dxContext.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createDepthTextureArraySRV(dx_texture& texture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = getReadFormatFromTypeless(texture.resource->GetDesc().Format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = texture.resource->GetDesc().DepthOrArraySize;

	dxContext.device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, cpuHandle);

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

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createBufferSRV(dx_buffer& buffer, buffer_range bufferRange)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = bufferRange.firstElement;
	srvDesc.Buffer.NumElements = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	srvDesc.Buffer.StructureByteStride = buffer.elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	dxContext.device->CreateShaderResourceView(buffer.resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createRawBufferSRV(dx_buffer& buffer, buffer_range bufferRange)
{
	uint32 firstElementByteOffset = bufferRange.firstElement * buffer.elementSize;
	assert(firstElementByteOffset % 16 == 0);

	uint32 count = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	uint32 totalSize = count * buffer.elementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = firstElementByteOffset / 4;
	srvDesc.Buffer.NumElements = totalSize / 4;
	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	dxContext.device->CreateShaderResourceView(buffer.resource.Get(), &srvDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::create2DTextureUAV(dx_texture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2D.MipSlice = mipSlice;
	dxContext.device->CreateUnorderedAccessView(texture.resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::create2DTextureArrayUAV(dx_texture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = texture.resource->GetDesc().DepthOrArraySize;
	uavDesc.Texture2DArray.MipSlice = mipSlice;
	dxContext.device->CreateUnorderedAccessView(texture.resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createCubemapUAV(dx_texture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = texture.resource->GetDesc().DepthOrArraySize;
	uavDesc.Texture2DArray.MipSlice = mipSlice;
	dxContext.device->CreateUnorderedAccessView(texture.resource.Get(), 0, &uavDesc, cpuHandle);

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

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createBufferUAV(dx_buffer& buffer, buffer_range bufferRange)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.FirstElement = bufferRange.firstElement;
	uavDesc.Buffer.NumElements = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	uavDesc.Buffer.StructureByteStride = buffer.elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxContext.device->CreateUnorderedAccessView(buffer.resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createBufferUintUAV(dx_buffer& buffer, buffer_range bufferRange)
{
	uint32 firstElementByteOffset = bufferRange.firstElement * buffer.elementSize;
	assert(firstElementByteOffset % 16 == 0);

	uint32 count = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	uint32 totalSize = count * buffer.elementSize;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_UINT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.FirstElement = firstElementByteOffset / 4;
	uavDesc.Buffer.NumElements = totalSize / 4;
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxContext.device->CreateUnorderedAccessView(buffer.resource.Get(), 0, &uavDesc, cpuHandle);

	return *this;
}

dx_cpu_descriptor_handle& dx_cpu_descriptor_handle::createRaytracingAccelerationStructureSRV(dx_buffer& tlas)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlas.gpuVirtualAddress;

	dxContext.device->CreateShaderResourceView(nullptr, &srvDesc, cpuHandle);

	return *this;
}
