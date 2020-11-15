#pragma once

#include "dx.h"

struct dx_texture;
struct dx_buffer;


struct texture_mip_range
{
	uint32 first = 0;
	uint32 count = (uint32)-1; // Use all mips.
};

struct buffer_range
{
	uint32 firstElement = 0;
	uint32 numElements = (uint32)-1; // Use all elements.
};

struct dx_cpu_descriptor_handle
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;

	inline operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return cpuHandle; }

	dx_cpu_descriptor_handle& create2DTextureSRV(dx_texture& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createCubemapSRV(dx_texture& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createCubemapArraySRV(dx_texture& texture, texture_mip_range mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createDepthTextureSRV(dx_texture& texture);
	dx_cpu_descriptor_handle& createDepthTextureArraySRV(dx_texture& texture);
	dx_cpu_descriptor_handle& createNullTextureSRV();
	dx_cpu_descriptor_handle& createBufferSRV(dx_buffer& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createRawBufferSRV(dx_buffer& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& create2DTextureUAV(dx_texture& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& create2DTextureArrayUAV(dx_texture& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createCubemapUAV(dx_texture& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createNullTextureUAV();
	dx_cpu_descriptor_handle& createBufferUAV(dx_buffer& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createBufferUintUAV(dx_buffer& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createRaytracingAccelerationStructureSRV(dx_buffer& tlas);
};

struct dx_gpu_descriptor_handle
{
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	inline operator CD3DX12_GPU_DESCRIPTOR_HANDLE() const { return gpuHandle; }
};

struct dx_double_descriptor_handle : dx_cpu_descriptor_handle, dx_gpu_descriptor_handle
{
};
