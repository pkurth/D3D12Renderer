#include "pch.h"
#include "render_resources.h"
#include "dx/dx_context.h"
#include "dx/dx_barrier_batcher.h"
#include "texture_preprocessing.h"
#include "core/hash.h"
#include "render_algorithms.h"


ref<dx_texture> render_resources::brdfTex;
ref<dx_texture> render_resources::whiteTexture;
ref<dx_texture> render_resources::blackTexture;
ref<dx_texture> render_resources::blackCubeTexture;
ref<dx_texture> render_resources::noiseTexture;
				
ref<dx_texture> render_resources::shadowMap;
ref<dx_texture> render_resources::staticShadowMapCache;

dx_cpu_descriptor_handle render_resources::nullTextureSRV;
dx_cpu_descriptor_handle render_resources::nullBufferSRV;

dx_rtv_descriptor_handle render_resources::nullScreenVelocitiesRTV;
dx_rtv_descriptor_handle render_resources::nullObjectIDsRTV;

dx_heap render_resources::resourceHeap;
std::unordered_map<uint64, render_resources::temporary_render_resources> render_resources::resourceMap;

bool render_resources::dirty = false;

void render_resources::initializeGlobalResources()
{
	{
		uint8 white[] = { 255, 255, 255, 255 };
		whiteTexture = createTexture(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(whiteTexture->resource, "White");
	}
	{
		uint8 black[] = { 0, 0, 0, 255 };
		blackTexture = createTexture(black, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(blackTexture->resource, "Black");

		blackCubeTexture = createCubeTexture(black, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(blackCubeTexture->resource, "Black cube");
	}

	noiseTexture = loadTextureFromFile("resources/noise/blue_noise.dds", image_load_flags_noncolor); // Already compressed and in DDS format.

	shadowMap = createDepthTexture(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, shadowDepthFormat, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	SET_NAME(shadowMap->resource, "Shadow map");

	staticShadowMapCache = createDepthTexture(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, shadowDepthFormat, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	SET_NAME(staticShadowMapCache->resource, "Static shadow map cache");

	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		brdfTex = integrateBRDF(cl);
		dxContext.executeCommandList(cl);
	}

	nullTextureSRV = dx_cpu_descriptor_handle(dxContext.srvUavAllocator.allocate(1).cpuAt(0)).createNullTextureSRV();
	nullBufferSRV = dx_cpu_descriptor_handle(dxContext.srvUavAllocator.allocate(1).cpuAt(0)).createNullBufferSRV();

	nullScreenVelocitiesRTV = dx_rtv_descriptor_handle(dxContext.rtvAllocator.allocate(1).cpuAt(0)).createNullTextureRTV(screenVelocitiesFormat);
	nullObjectIDsRTV = dx_rtv_descriptor_handle(dxContext.rtvAllocator.allocate(1).cpuAt(0)).createNullTextureRTV(objectIDsFormat);
}

void render_resources::declareTemporaryResourceNeeds(uint64 id, const std::vector<render_resource_desc>& descs)
{
	uint64 hash = hashResourceRequirements(descs);

	auto it = resourceMap.find(id);
	if (it == resourceMap.end() || it->second.hash != hash)
	{
		dirty = true;

		temporary_render_resources resources;
		resources.hash = hash;
		resources.descs = descs;
		getAllocationInfo(resources);
		resourceMap[id] = std::move(resources);
	}
}

void render_resources::evaluate()
{
	if (dirty)
	{
		if (resourceHeap)
		{
			dxContext.retire(resourceHeap);
		}

		uint64 maxAlignment = 0;
		uint64 maxSize = 0;
		for (auto it : resourceMap)
		{
			maxAlignment = max(maxAlignment, it.second.alignment);
			maxSize = max(maxSize, it.second.totalAllocationSize);
		}

		D3D12_HEAP_DESC heapDesc = {};
		heapDesc.SizeInBytes = maxSize;
		heapDesc.Alignment = maxAlignment;
		heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

		checkResult(dxContext.device->CreateHeap(&heapDesc, IID_PPV_ARGS(&resourceHeap)));


		for (auto it : resourceMap)
		{
			temporary_render_resources& resources = it.second;
			allocateResources(resources);
		}

		dirty = false;
	}
}

const std::vector<ref<dx_texture>>& render_resources::getTemporaryResources(uint64 id, dx_command_list* cl)
{
	assert(!dirty);

	std::vector<ref<dx_texture>>& textures = resourceMap[id].textures;
	
	barrier_batcher batcher(cl);
	for (const ref<dx_texture>& texture : textures)
	{
		batcher.aliasing(0, texture);
	}

	return textures;
}

void render_resources::getAllocationInfo(temporary_render_resources& resources)
{
	resources.offsetsInBytes.clear();

	uint64 offset = 0;
	for (const render_resource_desc& desc : resources.descs)
	{
		D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = dxContext.device->GetResourceAllocationInfo(0, 1, &desc.desc);

		if (offset == 0)
		{
			resources.alignment = allocationInfo.Alignment;
		}

		offset = alignTo(offset, allocationInfo.Alignment);

		resources.offsetsInBytes.push_back(offset);

		offset += allocationInfo.SizeInBytes;
	}

	resources.totalAllocationSize = offset;
}

uint64 render_resources::hashResourceRequirements(const std::vector<render_resource_desc>& descs)
{
	uint64 hash = 0;
	for (const render_resource_desc& desc : descs)
	{
		hash_combine(hash, (uint32)desc.desc.Dimension);
		hash_combine(hash, desc.desc.Alignment);
		hash_combine(hash, desc.desc.Width);
		hash_combine(hash, desc.desc.Height);
		hash_combine(hash, desc.desc.DepthOrArraySize);
		hash_combine(hash, desc.desc.MipLevels);
		hash_combine(hash, desc.desc.Format);
		hash_combine(hash, desc.desc.SampleDesc.Count);
		hash_combine(hash, desc.desc.SampleDesc.Quality);
		hash_combine(hash, (uint32)desc.desc.Layout);
		hash_combine(hash, (uint32)desc.desc.Flags);
	}
	return hash;
}

void render_resources::allocateResources(temporary_render_resources& resources)
{
	resources.textures.clear();

	for (uint32 i = 0; i < (uint32)resources.descs.size(); ++i)
	{
		uint64 offset = resources.offsetsInBytes[i];
		auto& desc = resources.descs[i];

		ref<dx_texture> texture =
			isDepthFormat(desc.desc.Format)
			? createPlacedDepthTexture(resourceHeap, offset, desc.desc, desc.initialState)
			: createPlacedTexture(resourceHeap, offset, desc.desc, desc.initialState);
		texture->setName(desc.name);

		resources.textures.push_back(texture);
	}
}
