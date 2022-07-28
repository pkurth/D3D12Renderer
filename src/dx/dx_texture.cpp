#include "pch.h"
#include "dx_texture.h"
#include "dx_context.h"
#include "dx_command_list.h"
#include "core/hash.h"
#include "rendering/texture_preprocessing.h"

#include <d3d12memoryallocator/D3D12MemAlloc.h>



static ref<dx_texture> uploadImageToGPU(DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc, uint32 flags)
{
	const DirectX::Image* images = scratchImage.GetImages();
	uint32 numImages = (uint32)scratchImage.GetImageCount();

	D3D12_SUBRESOURCE_DATA subresources[128];
	for (uint32 i = 0; i < numImages; ++i)
	{
		D3D12_SUBRESOURCE_DATA& subresource = subresources[i];
		subresource.RowPitch = images[i].rowPitch;
		subresource.SlicePitch = images[i].slicePitch;
		subresource.pData = images[i].pixels;
	}

	ref<dx_texture> result = createTexture(textureDesc, subresources, numImages);
	SET_NAME(result->resource, "Loaded from file");

	if (flags & image_load_flags_gen_mips_on_gpu)
	{
		dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		generateMipMapsOnGPU(cl, result);
		dxContext.executeCommandList(cl);
	}

	return result;
}

static ref<dx_texture> loadTextureInternal(const fs::path& path, uint32 flags)
{
	if (flags & image_load_flags_gen_mips_on_gpu)
	{
		flags &= ~image_load_flags_gen_mips_on_cpu;
		flags |= image_load_flags_allocate_full_mipchain;
	}

	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	if (path.extension() == ".svg")
	{
		if (!loadSVGFromFile(path, flags, scratchImage, textureDesc))
		{
			return 0;
		}
	}
	else if (!loadImageFromFile(path, flags, scratchImage, textureDesc))
	{
		return 0;
	}

	return uploadImageToGPU(scratchImage, textureDesc, flags);
}

static ref<dx_texture> loadTextureFromMemoryInternal(const void* ptr, uint32 size, image_format imageFormat, const fs::path& cachePath, uint32 flags)
{
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	if (!loadImageFromMemory(ptr, size, imageFormat, cachePath, flags, scratchImage, textureDesc))
	{
		return nullptr;
	}

	return uploadImageToGPU(scratchImage, textureDesc, flags);
}

static ref<dx_texture> loadVolumeTextureInternal(const fs::path& dirname, uint32 flags)
{
	// No mip maps allowed for now!
	assert(!(flags & image_load_flags_allocate_full_mipchain));
	assert(!(flags & image_load_flags_gen_mips_on_cpu));
	assert(!(flags & image_load_flags_gen_mips_on_gpu));

	std::vector<DirectX::ScratchImage> scratchImages;
	D3D12_RESOURCE_DESC textureDesc = {};

	uint32 totalSize = 0;

	for (auto& p : fs::directory_iterator(dirname))
	{
		auto& path = p.path();
		DirectX::ScratchImage& s = scratchImages.emplace_back();
		if (!loadImageFromFile(p, flags, s, textureDesc))
		{
			return nullptr;
		}

		assert(s.GetImageCount() == 1);
		const auto& image = s.GetImages()[0];

		if (scratchImages.size() > 1)
		{
			assert(image.width == scratchImages.begin()->GetImages()[0].width);
			assert(image.height == scratchImages.begin()->GetImages()[0].height);
			assert(image.slicePitch == scratchImages.begin()->GetImages()[0].slicePitch);
		}

		totalSize += (uint32)image.slicePitch;
	}

	uint32 width = (uint32)textureDesc.Width;
	uint32 height = textureDesc.Height;
	uint32 depth = (uint32)scratchImages.size();

	uint8* allPixels = new uint8[totalSize];

	for (uint32 i = 0; i < depth; ++i)
	{
		DirectX::ScratchImage& s = scratchImages[i];
		const auto& image = s.GetImages()[0];

		memcpy(allPixels + i * image.slicePitch, image.pixels, image.slicePitch);
	}

	D3D12_SUBRESOURCE_DATA subresource;
	subresource.RowPitch = scratchImages.begin()->GetImages()[0].rowPitch;
	subresource.SlicePitch = scratchImages.begin()->GetImages()[0].slicePitch;
	subresource.pData = allPixels;

	ref<dx_texture> result = createVolumeTexture(0, width, height, depth, textureDesc.Format, false);
	uploadTextureSubresourceData(result, &subresource, 0, 1);

	delete[] allPixels;

	return result;
}

static std::unordered_map<fs::path, weakref<dx_texture>> textureCache; // TODO: Pack flags into key.
static std::mutex mutex;

ref<dx_texture> loadTextureFromFile(const fs::path& filename, uint32 flags)
{
	mutex.lock();

	auto sp = textureCache[filename].lock();
	if (!sp)
	{
		textureCache[filename] = sp = loadTextureInternal(filename, flags);
	}

	mutex.unlock();
	return sp;
}

ref<dx_texture> loadTextureFromMemory(const void* ptr, uint32 size, image_format imageFormat, const fs::path& cacheFilename, uint32 flags)
{
	mutex.lock();

	auto sp = textureCache[cacheFilename].lock();
	if (!sp)
	{
		textureCache[cacheFilename] = sp = loadTextureFromMemoryInternal(ptr, size, imageFormat, cacheFilename, flags);
	}

	mutex.unlock();
	return sp;
}

ref<dx_texture> loadVolumeTextureFromDirectory(const fs::path& dirname, uint32 flags)
{
	mutex.lock();

	auto sp = textureCache[dirname].lock();
	if (!sp)
	{
		textureCache[dirname] = sp = loadVolumeTextureInternal(dirname, flags);
	}

	mutex.unlock();
	return sp;
}

static bool checkFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT1 support)
{
	return (formatSupport.Support1 & support) != 0;
}

static bool checkFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT2 support)
{
	return (formatSupport.Support2 & support) != 0;
}

static bool formatSupportsRTV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport)
{
	return checkFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
}

static bool formatSupportsDSV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport)
{
	return checkFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
}

static bool formatSupportsSRV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport)
{
	return checkFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
}

static bool formatSupportsUAV(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport)
{
	return checkFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
		checkFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
		checkFormatSupport(formatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
}

D3D12_RESOURCE_ALLOCATION_INFO getTextureAllocationInfo(uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, D3D12_RESOURCE_FLAGS flags)
{
	uint32 numMips = allocateMips ? 0 : 1;
	auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, numMips, 1, 0, flags);
	return dxContext.device->GetResourceAllocationInfo(0, 1, &desc);
}

void uploadTextureSubresourceData(ref<dx_texture> texture, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources)
{
	dx_command_list* cl = dxContext.getFreeCopyCommandList();
	cl->transitionBarrier(texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	uint64 requiredSize = GetRequiredIntermediateSize(texture->resource.Get(), firstSubresource, numSubresources);

	dx_resource intermediateResource;

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);

#if !USE_D3D12_BLOCK_ALLOCATOR

	auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	checkResult(dxContext.device->CreateCommittedResource(
		&heapDesc,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)
	));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		&allocation,
		IID_PPV_ARGS(&intermediateResource)));
	dxContext.retire(allocation);
#endif

	UpdateSubresources<128>(cl->commandList.Get(), texture->resource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);
	dxContext.retire(intermediateResource);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.
	//cl->transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(cl);
}

ref<dx_texture> createTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState, bool mipUAVs)
{
	ref<dx_texture> result = make_ref<dx_texture>();

	result->requestedNumMipLevels = textureDesc.MipLevels;

	uint32 maxNumMipLevels = (uint32)log2((float)max((uint32)textureDesc.Width, textureDesc.Height)) + 1;
	textureDesc.MipLevels = min(maxNumMipLevels, result->requestedNumMipLevels);

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
	formatSupport.Format = textureDesc.Format;
	checkResult(dxContext.device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	result->supportsRTV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) && formatSupportsRTV(formatSupport);
	result->supportsDSV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) && formatSupportsDSV(formatSupport);
	result->supportsUAV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) && formatSupportsUAV(formatSupport);
	result->supportsSRV = formatSupportsSRV(formatSupport);

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET && !result->supportsRTV)
	{
		std::cerr << "Warning. Requested RTV, but not supported by format.\n";
		__debugbreak();
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL && !result->supportsDSV)
	{
		std::cerr << "Warning. Requested DSV, but not supported by format.\n";
		__debugbreak();
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS && !result->supportsUAV)
	{
		std::cerr << "Warning. Requested UAV, but not supported by format.\n";
		__debugbreak();
	}


	// Create.


#if !USE_D3D12_BLOCK_ALLOCATOR

	auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	checkResult(dxContext.device->CreateCommittedResource(&heapDesc,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		0,
		IID_PPV_ARGS(&result->resource)));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&textureDesc,
		initialState,
		0,
		&allocation,
		IID_PPV_ARGS(&result->resource)));

	result->allocation = allocation;
#endif


	result->numMipLevels = result->resource->GetDesc().MipLevels;

	result->format = textureDesc.Format;
	result->width = (uint32)textureDesc.Width;
	result->height = textureDesc.Height;
	result->depth = textureDesc.DepthOrArraySize;


	result->initialState = initialState;


	// Upload.
	if (subresourceData)
	{
		uploadTextureSubresourceData(result, subresourceData, 0, numSubresources);
	}

	uint32 numUAVMips = 0;
	if (result->supportsUAV)
	{
		numUAVMips = (mipUAVs ? result->numMipLevels : 1);
	}
	result->srvUavAllocation = dxContext.srvUavAllocator.allocate(1 + numUAVMips);

	// SRV.
	if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).createVolumeTextureSRV(result);
	}
	else if (textureDesc.DepthOrArraySize == 6)
	{
		result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).createCubemapSRV(result);
	}
	else
	{
		result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).create2DTextureSRV(result);
	}

	// RTV.
	if (result->supportsRTV)
	{
		result->rtvAllocation = dxContext.rtvAllocator.allocate(textureDesc.DepthOrArraySize);
		for (uint32 i = 0; i < textureDesc.DepthOrArraySize; ++i)
		{
			dx_rtv_descriptor_handle(result->rtvAllocation.cpuAt(i)).create2DTextureRTV(result, i);
		}
		result->defaultRTV = result->rtvAllocation.cpuAt(0);
	}

	// UAV.
	if (result->supportsUAV)
	{
		result->defaultUAV = result->srvUavAllocation.cpuAt(1);

		for (uint32 i = 0; i < numUAVMips; ++i)
		{
			if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
			{
				dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1 + i)).createVolumeTextureUAV(result, i);
			}
			else if (textureDesc.DepthOrArraySize == 6)
			{
				dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1 + i)).createCubemapUAV(result, i);
			}
			else
			{
				dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1 + i)).create2DTextureUAV(result, i);
			}
		}
	}

	return result;
}

ref<dx_texture> createTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState, bool mipUAVs)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		;

	uint32 numMips = allocateMips ? 0 : 1;
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, numMips, 1, 0, flags);

	if (data)
	{
		uint32 formatSize = getFormatSize(textureDesc.Format);

		D3D12_SUBRESOURCE_DATA subresource;
		subresource.RowPitch = width * formatSize;
		subresource.SlicePitch = width * height * formatSize;
		subresource.pData = data;

		return createTexture(textureDesc, &subresource, 1, initialState, mipUAVs);
	}
	else
	{
		return createTexture(textureDesc, 0, 0, initialState, mipUAVs);
	}
}

static void initializeDepthTexture(ref<dx_texture> result, uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength, D3D12_RESOURCE_STATES initialState, bool allocateDescriptors)
{
	result->numMipLevels = 1;
	result->requestedNumMipLevels = 1;
	result->format = format;
	result->width = width;
	result->height = height;
	result->depth = arrayLength;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
	formatSupport.Format = format;
	checkResult(dxContext.device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	result->supportsRTV = false;
	result->supportsDSV = allocateDescriptors && formatSupportsDSV(formatSupport);
	result->supportsUAV = false;
	result->supportsSRV = allocateDescriptors && formatSupportsSRV(formatSupport);


	result->initialState = initialState;

	if (allocateDescriptors)
	{
		assert(result->supportsDSV);

		result->dsvAllocation = dxContext.dsvAllocator.allocate();
		result->defaultDSV = dx_dsv_descriptor_handle(result->dsvAllocation.cpuAt(0)).create2DTextureDSV(result);

		if (arrayLength == 1)
		{
			result->srvUavAllocation = dxContext.srvUavAllocator.allocate(1 + isStencilFormat(format));
			result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).createDepthTextureSRV(result);

			if (isStencilFormat(format))
			{
				result->stencilSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1)).createStencilTextureSRV(result);
			}
		}
		else
		{
			result->srvUavAllocation = dxContext.srvUavAllocator.allocate();
			result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).createDepthTextureArraySRV(result);
		}
	}
}

ref<dx_texture> createDepthTexture(uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_texture> result = make_ref<dx_texture>();

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(getTypelessFormat(format), width, height,
		arrayLength, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

#if !USE_D3D12_BLOCK_ALLOCATOR

	auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	checkResult(dxContext.device->CreateCommittedResource(
		&heapDesc,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		&optimizedClearValue,
		IID_PPV_ARGS(&result->resource)
	));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&desc,
		initialState,
		&optimizedClearValue,
		&allocation,
		IID_PPV_ARGS(&result->resource)));

	result->allocation = allocation;
#endif

	initializeDepthTexture(result, width, height, format, arrayLength, initialState, true);

	return result;
}

ref<dx_texture> createCubeTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState, bool mipUAVs)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		;

	uint32 numMips = allocateMips ? 0 : 1;
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 6, numMips, 1, 0, flags);

	if (data)
	{
		uint32 formatSize = getFormatSize(textureDesc.Format);

		D3D12_SUBRESOURCE_DATA subresources[6];
		for (uint32 i = 0; i < 6; ++i)
		{
			auto& subresource = subresources[i];
			subresource.RowPitch = width * formatSize;
			subresource.SlicePitch = width * height * formatSize;
			subresource.pData = data;
		}

		return createTexture(textureDesc, subresources, 6, initialState, mipUAVs);
	}
	else
	{
		return createTexture(textureDesc, 0, 0, initialState, mipUAVs);
	}
}

ref<dx_texture> createVolumeTexture(const void* data, uint32 width, uint32 height, uint32 depth, DXGI_FORMAT format, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		;

	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(format, width, height, depth, 1, flags);

	if (data)
	{
		uint32 formatSize = getFormatSize(textureDesc.Format);

		D3D12_SUBRESOURCE_DATA* subresources = (D3D12_SUBRESOURCE_DATA*)alloca(sizeof(D3D12_SUBRESOURCE_DATA) * depth);
		for (uint32 i = 0; i < depth; ++i)
		{
			auto& subresource = subresources[i];
			subresource.RowPitch = width * formatSize;
			subresource.SlicePitch = width * height * formatSize;
			subresource.pData = data;
		}

		return createTexture(textureDesc, subresources, depth, initialState);
	}
	else
	{
		return createTexture(textureDesc, 0, 0, initialState);
	}
}

void dx_texture::setName(const wchar* name)
{
	checkResult(resource->SetName(name));
}

std::wstring dx_texture::getName() const
{
	if (!resource)
	{
		return L"";
	}

	wchar name[128];
	uint32 size = sizeof(name); 
	resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name); 
	name[min((uint32)arraysize(name) - 1, size)] = 0;

	return name;
}

ref<dx_texture> createPlacedTexture(dx_heap heap, uint64 offset, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState, bool mipUAVs)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		;

	uint32 numMips = allocateMips ? 0 : 1;
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, numMips, 1, 0, flags);
	
	return createPlacedTexture(heap, offset, textureDesc, initialState, mipUAVs);
}

ref<dx_texture> createPlacedTexture(dx_heap heap, uint64 offset, D3D12_RESOURCE_DESC textureDesc, D3D12_RESOURCE_STATES initialState, bool mipUAVs)
{
	ref<dx_texture> result = make_ref<dx_texture>();

	result->requestedNumMipLevels = textureDesc.MipLevels;

	uint32 maxNumMipLevels = (uint32)log2((float)max((uint32)textureDesc.Width, textureDesc.Height)) + 1;
	textureDesc.MipLevels = min(maxNumMipLevels, result->requestedNumMipLevels);

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
	formatSupport.Format = textureDesc.Format;
	checkResult(dxContext.device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	result->supportsRTV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) && formatSupportsRTV(formatSupport);
	result->supportsDSV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) && formatSupportsDSV(formatSupport);
	result->supportsUAV = (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) && formatSupportsUAV(formatSupport);
	result->supportsSRV = formatSupportsSRV(formatSupport);

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET && !result->supportsRTV)
	{
		std::cerr << "Warning. Requested RTV, but not supported by format.\n";
		__debugbreak();
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL && !result->supportsDSV)
	{
		std::cerr << "Warning. Requested DSV, but not supported by format.\n";
		__debugbreak();
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS && !result->supportsUAV)
	{
		std::cerr << "Warning. Requested UAV, but not supported by format.\n";
		__debugbreak();
	}


	// Create.
	checkResult(dxContext.device->CreatePlacedResource(
		heap.Get(),
		offset,
		&textureDesc,
		initialState,
		0,
		IID_PPV_ARGS(&result->resource)
	));


	result->numMipLevels = result->resource->GetDesc().MipLevels;

	result->format = textureDesc.Format;
	result->width = (uint32)textureDesc.Width;
	result->height = textureDesc.Height;
	result->depth = textureDesc.DepthOrArraySize;

	result->initialState = initialState;


	uint32 numUAVMips = 0;
	if (result->supportsUAV)
	{
		numUAVMips = (mipUAVs ? result->numMipLevels : 1);
	}
	result->srvUavAllocation = dxContext.srvUavAllocator.allocate(1 + numUAVMips);

	// SRV.
	if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).createVolumeTextureSRV(result);
	}
	else if (textureDesc.DepthOrArraySize == 6)
	{
		result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).createCubemapSRV(result);
	}
	else
	{
		result->defaultSRV = dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(0)).create2DTextureSRV(result);
	}

	// RTV.
	if (result->supportsRTV)
	{
		result->rtvAllocation = dxContext.rtvAllocator.allocate(textureDesc.DepthOrArraySize);
		for (uint32 i = 0; i < textureDesc.DepthOrArraySize; ++i)
		{
			dx_rtv_descriptor_handle(result->rtvAllocation.cpuAt(i)).create2DTextureRTV(result, i);
		}
		result->defaultRTV = result->rtvAllocation.cpuAt(0);
	}

	// UAV.
	if (result->supportsUAV)
	{
		result->defaultUAV = result->srvUavAllocation.cpuAt(1);

		for (uint32 i = 0; i < numUAVMips; ++i)
		{
			if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
			{
				dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1 + i)).createVolumeTextureUAV(result, i);
			}
			else if (textureDesc.DepthOrArraySize == 6)
			{
				dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1 + i)).createCubemapUAV(result, i);
			}
			else
			{
				dx_cpu_descriptor_handle(result->srvUavAllocation.cpuAt(1 + i)).create2DTextureUAV(result, i);
			}
		}
	}

	return result;
}

ref<dx_texture> createPlacedDepthTexture(dx_heap heap, uint64 offset, uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength, D3D12_RESOURCE_STATES initialState, bool allowDepthStencil)
{
	ref<dx_texture> result = make_ref<dx_texture>();

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(getTypelessFormat(format), width, height,
		arrayLength, 1, 1, 0, allowDepthStencil ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	checkResult(dxContext.device->CreatePlacedResource(
		heap.Get(),
		offset,
		&desc,
		initialState,
		allowDepthStencil ? &optimizedClearValue : 0,
		IID_PPV_ARGS(&result->resource)
	));

	initializeDepthTexture(result, width, height, format, arrayLength, initialState, allowDepthStencil);

	return result;
}

ref<dx_texture> createPlacedDepthTexture(dx_heap heap, uint64 offset, D3D12_RESOURCE_DESC textureDesc, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_texture> result = make_ref<dx_texture>();

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = textureDesc.Format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };

	checkResult(dxContext.device->CreatePlacedResource(
		heap.Get(),
		offset,
		&textureDesc,
		initialState,
		&optimizedClearValue,
		IID_PPV_ARGS(&result->resource)
	));

	initializeDepthTexture(result, (uint32)textureDesc.Width, textureDesc.Height, textureDesc.Format, textureDesc.DepthOrArraySize, initialState, true);

	return result;
}

static void retire(dx_resource resource, dx_descriptor_allocation srvUavAllocation, dx_descriptor_allocation rtvAllocation, dx_descriptor_allocation dsvAllocation)
{
	texture_grave grave;
	grave.resource = resource;
	grave.srvUavAllocation = srvUavAllocation;
	grave.rtvAllocation = rtvAllocation;
	grave.dsvAllocation = dsvAllocation;
	dxContext.retire(std::move(grave));
}

dx_texture::~dx_texture()
{
	retire(resource, srvUavAllocation, rtvAllocation, dsvAllocation);
	if (allocation)
	{
		dxContext.retire(allocation);
	}
}

void resizeTexture(ref<dx_texture> texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState)
{
	if (!texture)
	{
		return;
	}

	wchar name[128];
	uint32 size = sizeof(name);
	texture->resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
	name[min((uint32)arraysize(name) - 1, size)] = 0;

	bool hasMipUAVs = texture->srvUavAllocation.count > 2;

	retire(texture->resource, texture->srvUavAllocation, texture->rtvAllocation, texture->dsvAllocation);
	if (texture->allocation)
	{
		dxContext.retire(texture->allocation);
	}

	D3D12_RESOURCE_DESC desc = texture->resource->GetDesc();
	texture->resource.Reset();


	D3D12_RESOURCE_STATES state = (initialState == -1) ? texture->initialState : initialState;
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	D3D12_CLEAR_VALUE* clearValue = 0;

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		optimizedClearValue.Format = texture->format;
		optimizedClearValue.DepthStencil = { 1.f, 0 };
		clearValue = &optimizedClearValue;
	}

	uint32 maxNumMipLevels = (uint32)log2((float)max(newWidth, newHeight)) + 1;
	desc.MipLevels = min(maxNumMipLevels, texture->requestedNumMipLevels);

	desc.Width = newWidth;
	desc.Height = newHeight;
	texture->width = newWidth;
	texture->height = newHeight;

#if !USE_D3D12_BLOCK_ALLOCATOR

	auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	checkResult(dxContext.device->CreateCommittedResource(
		&heapDesc,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(&texture->resource)
	));
#else
	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12MA::Allocation* allocation;
	checkResult(dxContext.memoryAllocator->CreateResource(
		&allocationDesc,
		&desc,
		state,
		clearValue,
		&allocation,
		IID_PPV_ARGS(&texture->resource)));

	texture->allocation = allocation;
#endif

	texture->numMipLevels = texture->resource->GetDesc().MipLevels;


	uint32 numUAVMips = 0;
	if (texture->supportsUAV)
	{
		numUAVMips = (hasMipUAVs ? texture->numMipLevels : 1);
	}



	// RTV.
	if (texture->supportsRTV)
	{
		texture->rtvAllocation = dxContext.rtvAllocator.allocate();
		texture->defaultRTV = dx_rtv_descriptor_handle(texture->rtvAllocation.cpuAt(0)).create2DTextureRTV(texture);
	}

	// DSV & SRV.
	if (texture->supportsDSV)
	{
		texture->dsvAllocation = dxContext.dsvAllocator.allocate(1);
		texture->defaultDSV = dx_dsv_descriptor_handle(texture->dsvAllocation.cpuAt(0)).create2DTextureDSV(texture);

		texture->srvUavAllocation = dxContext.srvUavAllocator.allocate(1 + isStencilFormat(texture->format));
		if (texture->depth == 1)
		{
			texture->defaultSRV = dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(0)).createDepthTextureSRV(texture);
		}
		else
		{
			texture->defaultSRV = dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(0)).createDepthTextureArraySRV(texture);
		}

		if (isStencilFormat(texture->format))
		{
			texture->stencilSRV = dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(1)).createStencilTextureSRV(texture);
		}
	}
	else
	{
		texture->srvUavAllocation = dxContext.srvUavAllocator.allocate(1 + numUAVMips);

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			texture->defaultSRV = dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(0)).createVolumeTextureSRV(texture);
		}
		else if (texture->depth == 6)
		{
			texture->defaultSRV = dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(0)).createCubemapSRV(texture);
		}
		else
		{
			texture->defaultSRV = dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(0)).create2DTextureSRV(texture);
		}


		// UAV.
		if (texture->supportsUAV)
		{
			texture->defaultUAV = texture->srvUavAllocation.cpuAt(1);

			for (uint32 i = 0; i < numUAVMips; ++i)
			{
				if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
				{
					dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(1 + i)).createVolumeTextureUAV(texture, i);
				}
				else if (desc.DepthOrArraySize == 6)
				{
					dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(1 + i)).createCubemapUAV(texture, i);
				}
				else
				{
					dx_cpu_descriptor_handle(texture->srvUavAllocation.cpuAt(1 + i)).create2DTextureUAV(texture, i);
				}
			}
		}
	}

	texture->setName(name);
}

texture_grave::~texture_grave()
{
	wchar name[128];

	if (resource)
	{
		uint32 size = sizeof(name);
		resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
		name[min((uint32)arraysize(name) - 1, size)] = 0;

		dxContext.srvUavAllocator.free(srvUavAllocation);
		dxContext.rtvAllocator.free(rtvAllocation);
		dxContext.dsvAllocator.free(dsvAllocation);
	}
}

void saveTextureToFile(const ref<dx_texture>& texture, const fs::path& path)
{
	saveTextureToFile(texture->resource, texture->width, texture->height, texture->format, path);
}

void saveTextureToFile(dx_resource texture, uint32 width, uint32 height, DXGI_FORMAT format, const fs::path& path)
{
	assert(format == DXGI_FORMAT_R8G8B8A8_UNORM);
	uint32 outputSize = 4;

	uint32 readbackPitch = alignTo(width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	ref<dx_buffer> readbackBuffer = createReadbackBuffer(outputSize, readbackPitch * height);

	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	cl->transitionBarrier(texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
	cl->copyTextureRegionToBuffer(texture, width, format, readbackBuffer, 0, 0, 0, width, height);
	cl->transitionBarrier(texture, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON);

	uint64 fence = dxContext.executeCommandList(cl);

	dxContext.renderQueue.waitForFence(fence);

	uint8* output = new uint8[width * height * outputSize];

	uint8* dest = output;
	uint32 destPitch = outputSize * width;

	uint8* result = (uint8*)mapBuffer(readbackBuffer, true);
	uint32 resultPitch = (uint32)alignTo(outputSize * width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	for (uint32 h = 0; h < height; ++h)
	{
		memcpy(dest, result, destPitch);
		result += resultPitch;
		dest += destPitch;
	}

	unmapBuffer(readbackBuffer, false);



	DirectX::Image image;
	image.width = width;
	image.height = height;
	image.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	image.rowPitch = width * getFormatSize(image.format);
	image.slicePitch = image.rowPitch * height;
	image.pixels = output;

	saveImageToFile(path, image);


	delete[] output;
}


