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
	dx_cpu_descriptor_handle() = default;
	dx_cpu_descriptor_handle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle) : cpuHandle(handle) {}
	dx_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) : cpuHandle(handle) {}
	dx_cpu_descriptor_handle(CD3DX12_DEFAULT) : cpuHandle(D3D12_DEFAULT) {}

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;

	inline operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return cpuHandle; }

	dx_cpu_descriptor_handle& create2DTextureSRV(const ref<dx_texture>& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createCubemapSRV(const ref<dx_texture>& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createCubemapArraySRV(const ref<dx_texture>& texture, texture_mip_range mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createVolumeTextureSRV(const ref<dx_texture>& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createDepthTextureSRV(const ref<dx_texture>& texture);
	dx_cpu_descriptor_handle& createDepthTextureArraySRV(const ref<dx_texture>& texture);
	dx_cpu_descriptor_handle& createStencilTextureSRV(const ref<dx_texture>& texture);
	dx_cpu_descriptor_handle& createNullTextureSRV();
	dx_cpu_descriptor_handle& createBufferSRV(const ref<dx_buffer>& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createRawBufferSRV(const ref<dx_buffer>& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createNullBufferSRV();
	dx_cpu_descriptor_handle& create2DTextureUAV(const ref<dx_texture>& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& create2DTextureArrayUAV(const ref<dx_texture>& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createCubemapUAV(const ref<dx_texture>& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createVolumeTextureUAV(const ref<dx_texture>& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_cpu_descriptor_handle& createNullTextureUAV();
	dx_cpu_descriptor_handle& createBufferUAV(const ref<dx_buffer>& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createBufferUintUAV(const ref<dx_buffer>& buffer, buffer_range bufferRange = {});
	dx_cpu_descriptor_handle& createNullBufferUAV();
	dx_cpu_descriptor_handle& createRaytracingAccelerationStructureSRV(const ref<dx_buffer>& tlas);


	dx_cpu_descriptor_handle operator+(uint32 i);
	dx_cpu_descriptor_handle& operator+=(uint32 i);
	dx_cpu_descriptor_handle& operator++();
	dx_cpu_descriptor_handle operator++(int);
};

struct dx_gpu_descriptor_handle
{
	dx_gpu_descriptor_handle() = default;
	dx_gpu_descriptor_handle(CD3DX12_GPU_DESCRIPTOR_HANDLE handle) : gpuHandle(handle) {}
	dx_gpu_descriptor_handle(D3D12_GPU_DESCRIPTOR_HANDLE handle) : gpuHandle(handle) {}
	dx_gpu_descriptor_handle(CD3DX12_DEFAULT) : gpuHandle(D3D12_DEFAULT) {}

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;

	inline operator CD3DX12_GPU_DESCRIPTOR_HANDLE() const { return gpuHandle; }


	dx_gpu_descriptor_handle operator+(uint32 i);
	dx_gpu_descriptor_handle& operator+=(uint32 i);
	dx_gpu_descriptor_handle& operator++();
	dx_gpu_descriptor_handle operator++(int);
};

struct dx_double_descriptor_handle : dx_cpu_descriptor_handle, dx_gpu_descriptor_handle
{
};

struct dx_rtv_descriptor_handle
{
	dx_rtv_descriptor_handle() = default;
	dx_rtv_descriptor_handle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle) : cpuHandle(handle) {}
	dx_rtv_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) : cpuHandle(handle) {}
	dx_rtv_descriptor_handle(CD3DX12_DEFAULT) : cpuHandle(D3D12_DEFAULT) {}

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;

	inline operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return cpuHandle; }


	dx_rtv_descriptor_handle& create2DTextureRTV(const ref<dx_texture>& texture, uint32 arraySlice = 0, uint32 mipSlice = 0);
};

struct dx_dsv_descriptor_handle
{
	dx_dsv_descriptor_handle() = default;
	dx_dsv_descriptor_handle(CD3DX12_CPU_DESCRIPTOR_HANDLE handle) : cpuHandle(handle) {}
	dx_dsv_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) : cpuHandle(handle) {}
	dx_dsv_descriptor_handle(CD3DX12_DEFAULT) : cpuHandle(D3D12_DEFAULT) {}

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;

	inline operator CD3DX12_CPU_DESCRIPTOR_HANDLE() const { return cpuHandle; }


	dx_dsv_descriptor_handle& create2DTextureDSV(const ref<dx_texture>& texture, uint32 arraySlice = 0, uint32 mipSlice = 0);
};
