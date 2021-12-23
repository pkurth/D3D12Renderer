#pragma once

#include "dx_descriptor.h"
#include "dx_descriptor_allocation.h"
#include "core/asset.h"
#include "core/math.h"
#include "core/image.h"

#include <string>


struct dx_texture
{
	virtual ~dx_texture();

	dx_resource resource;
	D3D12MA::Allocation* allocation = 0;


	dx_descriptor_allocation srvUavAllocation = {};
	dx_descriptor_allocation rtvAllocation = {};
	dx_descriptor_allocation dsvAllocation = {};



	dx_cpu_descriptor_handle defaultSRV; // SRV for the whole texture (all mip levels).
	dx_cpu_descriptor_handle defaultUAV; // UAV for the first mip level.
	dx_cpu_descriptor_handle uavAt(uint32 index) { return srvUavAllocation.cpuAt(1 + index); }

	dx_cpu_descriptor_handle stencilSRV; // For depth stencil textures.


	dx_rtv_descriptor_handle defaultRTV;
	dx_rtv_descriptor_handle rtvAt(uint32 index) { return rtvAllocation.cpuAt(index); }

	dx_dsv_descriptor_handle defaultDSV;


	uint32 width, height, depth;
	DXGI_FORMAT format;

	D3D12_RESOURCE_STATES initialState;

	bool supportsRTV;
	bool supportsDSV;
	bool supportsUAV;
	bool supportsSRV;

	uint32 requestedNumMipLevels;
	uint32 numMipLevels;

	asset_handle handle;

	void setName(const wchar* name);
	std::wstring getName() const;
};

struct dx_texture_atlas
{
	ref<dx_texture> texture;

	uint32 cols;
	uint32 rows;

	std::pair<vec2, vec2> getUVs(uint32 x, uint32 y)
	{
		assert(x < cols);
		assert(y < rows);

		float width = 1.f / cols;
		float height = 1.f / rows;
		vec2 uv0 = vec2(x * width, y * height);
		vec2 uv1 = vec2((x + 1) * width, (y + 1) * height);

		return { uv0, uv1 };
	}

	std::pair<vec2, vec2> getUVs(uint32 i)
	{
		uint32 x = i % cols;
		uint32 y = i / cols;
		return getUVs(x, y);
	}
};

struct texture_grave
{
	dx_resource resource;

	dx_descriptor_allocation srvUavAllocation = {};
	dx_descriptor_allocation rtvAllocation = {};
	dx_descriptor_allocation dsvAllocation = {};

	texture_grave() {}
	texture_grave(const texture_grave& o) = delete;
	texture_grave(texture_grave&& o) = default;

	texture_grave& operator=(const texture_grave& o) = delete;
	texture_grave& operator=(texture_grave&& o) = default;

	~texture_grave();
};

D3D12_RESOURCE_ALLOCATION_INFO getTextureAllocationInfo(uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, D3D12_RESOURCE_FLAGS flags);

void uploadTextureSubresourceData(ref<dx_texture> texture, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources);
ref<dx_texture> createTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, bool mipUAVs = false);
ref<dx_texture> createTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips = false, bool allowRenderTarget = false, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, bool mipUAVs = false);
ref<dx_texture> createDepthTexture(uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength = 1, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
ref<dx_texture> createCubeTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips = false, bool allowRenderTarget = false, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, bool mipUAVs = false);
ref<dx_texture> createVolumeTexture(const void* data, uint32 width, uint32 height, uint32 depth, DXGI_FORMAT format, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
void resizeTexture(ref<dx_texture> texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState = (D3D12_RESOURCE_STATES )-1);

ref<dx_texture> createPlacedTexture(dx_heap heap, uint64 offset, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips = false, bool allowRenderTarget = false, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, bool mipUAVs = false);
ref<dx_texture> createPlacedTexture(dx_heap heap, uint64 offset, D3D12_RESOURCE_DESC textureDesc, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, bool mipUAVs = false);

ref<dx_texture> createPlacedDepthTexture(dx_heap heap, uint64 offset, uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength = 1, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE, bool allowDepthStencil = true);
ref<dx_texture> createPlacedDepthTexture(dx_heap heap, uint64 offset, D3D12_RESOURCE_DESC textureDesc, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE);

// This system caches textures. It does not keep the resource alive (we store weak ptrs).
// So if no one else has a reference, the texture gets deleted.
// This means you should keep a reference to your textures yourself and not call this every frame.
// TODO: Maybe we want to keep the texture around for a couple more frames?

ref<dx_texture> loadTextureFromFile(const fs::path& filename, uint32 flags = image_load_flags_default);
ref<dx_texture> loadTextureFromMemory(const void* ptr, uint32 size, image_format imageFormat, const fs::path& cacheFilename, uint32 flags = image_load_flags_default);
ref<dx_texture> loadVolumeTextureFromDirectory(const fs::path& dirname, uint32 flags = image_load_flags_compress | image_load_flags_cache_to_dds | image_load_flags_noncolor);
