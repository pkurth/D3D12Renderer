#include "pch.h"
#include "dx_texture.h"
#include "dx_context.h"
#include "texture_preprocessing.h"
#include "dx_command_list.h"

#include <DirectXTex/DirectXTex.h>

#include <filesystem>

namespace fs = std::filesystem;



static DXGI_FORMAT makeSRGB(DXGI_FORMAT format)
{
	return DirectX::MakeSRGB(format);
}

static DXGI_FORMAT makeLinear(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;

	case DXGI_FORMAT_BC1_UNORM_SRGB:
		format = DXGI_FORMAT_BC1_UNORM;
		break;

	case DXGI_FORMAT_BC2_UNORM_SRGB:
		format = DXGI_FORMAT_BC2_UNORM;
		break;

	case DXGI_FORMAT_BC3_UNORM_SRGB:
		format = DXGI_FORMAT_BC3_UNORM;
		break;

	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;

	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		format = DXGI_FORMAT_B8G8R8X8_UNORM;
		break;

	case DXGI_FORMAT_BC7_UNORM_SRGB:
		format = DXGI_FORMAT_BC7_UNORM;
		break;
	}
	return format;
}

static void postProcessImage(DirectX::ScratchImage& scratchImage, DirectX::TexMetadata& metadata, uint32 flags, const fs::path& filepath, const fs::path& cacheFilepath)
{
	if (flags & texture_load_flags_noncolor)
	{
		metadata.format = makeLinear(metadata.format);
	}
	else
	{
		metadata.format = makeSRGB(metadata.format);
	}

	scratchImage.OverrideFormat(metadata.format);

	if (flags & texture_load_flags_gen_mips_on_cpu)
	{
		DirectX::ScratchImage mipchainImage;

		checkResult(DirectX::GenerateMipMaps(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::TEX_FILTER_DEFAULT, 0, mipchainImage));
		scratchImage = std::move(mipchainImage);
		metadata = scratchImage.GetMetadata();
	}
	else
	{
		metadata.mipLevels = 1;
	}

	if (flags & texture_load_flags_premultiply_alpha)
	{
		DirectX::ScratchImage premultipliedAlphaImage;

		checkResult(DirectX::PremultiplyAlpha(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::TEX_PMALPHA_DEFAULT, premultipliedAlphaImage));
		scratchImage = std::move(premultipliedAlphaImage);
		metadata = scratchImage.GetMetadata();
	}

	if (flags & texture_load_flags_compress)
	{
		if (metadata.width % 4 == 0 && metadata.height % 4 == 0)
		{
			if (!DirectX::IsCompressed(metadata.format))
			{
				uint32 numChannels = getNumberOfChannels(metadata.format);

				DXGI_FORMAT compressedFormat;

				switch (numChannels)
				{
					case 1: compressedFormat = DXGI_FORMAT_BC4_UNORM; break;
					case 2: compressedFormat = DXGI_FORMAT_BC5_UNORM; break;

					case 3:
					case 4:
					{
						if (scratchImage.IsAlphaAllOpaque())
						{
							compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
						}
						else
						{
							compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;  // 7 would be better, but takes forever to compress.
						}
					} break;
				}

				DirectX::ScratchImage compressedImage;

				checkResult(DirectX::Compress(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata,
					compressedFormat, DirectX::TEX_COMPRESS_PARALLEL, DirectX::TEX_THRESHOLD_DEFAULT, compressedImage));
				scratchImage = std::move(compressedImage);
				metadata = scratchImage.GetMetadata();
			}
		}
		else
		{
			std::cerr << "Cannot compress texture '" << filepath << "', since its dimensions are not a multiple of 4.\n";
		}
	}

	if (flags & texture_load_flags_cache_to_dds)
	{
		fs::create_directories(cacheFilepath.parent_path());
		checkResult(DirectX::SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::DDS_FLAGS_NONE, cacheFilepath.c_str()));
	}
}

static void createDesc(DirectX::TexMetadata& metadata, uint32 flags, D3D12_RESOURCE_DESC& textureDesc)
{
	if (flags & texture_load_flags_allocate_full_mipchain)
	{
		metadata.mipLevels = 0;
	}

	switch (metadata.dimension)
	{
		case DirectX::TEX_DIMENSION_TEXTURE1D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex1D(metadata.format, metadata.width, (uint16)metadata.arraySize, (uint16)metadata.mipLevels);
			break;
		case DirectX::TEX_DIMENSION_TEXTURE2D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width, (uint32)metadata.height, (uint16)metadata.arraySize, (uint16)metadata.mipLevels);
			break;
		case DirectX::TEX_DIMENSION_TEXTURE3D:
			textureDesc = CD3DX12_RESOURCE_DESC::Tex3D(metadata.format, metadata.width, (uint32)metadata.height, (uint16)metadata.depth, (uint16)metadata.mipLevels);
			break;
		default:
			assert(false);
			break;
	}
}

static bool loadImageFromFile(const fs::path& filepath, uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc)
{
	if (flags & texture_load_flags_gen_mips_on_gpu)
	{
		flags &= ~texture_load_flags_gen_mips_on_cpu;
		flags |= texture_load_flags_allocate_full_mipchain;
	}


	fs::path extension = filepath.extension();

	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension("." + std::to_string(flags) + ".cache.dds");

	fs::path cacheFilepath = L"asset_cache" / cachedFilename;

	bool fromCache = false;
	DirectX::TexMetadata metadata;

	if (!(flags & texture_load_flags_always_load_from_source))
	{
		// Look for cached.

		WIN32_FILE_ATTRIBUTE_DATA cachedData;
		if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData))
		{
			FILETIME cachedFiletime = cachedData.ftLastWriteTime;

			WIN32_FILE_ATTRIBUTE_DATA originalData;
			assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
			FILETIME originalFiletime = originalData.ftLastWriteTime;

			if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0)
			{
				// Cached file is newer than original, so load this.
				fromCache = SUCCEEDED(DirectX::LoadFromDDSFile(cacheFilepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
			}
		}
	}

	if (!fromCache)
	{
		if (!fs::exists(filepath))
		{
			std::cerr << "Could not find file '" << filepath.string() << "'.\n";
			return false;
		}

		if (flags & texture_load_flags_cache_to_dds)
		{
			std::cout << "Preprocessing asset '" << filepath.string() << "' for faster loading next time.";
#ifdef _DEBUG
			std::cout << " Consider running in a release build the first time.";
#endif
			std::cout << std::endl;
		}


		if (extension == ".dds")
		{
			if (FAILED(DirectX::LoadFromDDSFile(filepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage)))
			{
				return false;
			}
		}
		else if (extension == ".hdr")
		{
			if (FAILED(DirectX::LoadFromHDRFile(filepath.c_str(), &metadata, scratchImage)))
			{
				return false;
			}
		}
		else if (extension == ".tga")
		{
			if (FAILED(DirectX::LoadFromTGAFile(filepath.c_str(), &metadata, scratchImage)))
			{
				return false;
			}
		}
		else
		{
			if (FAILED(DirectX::LoadFromWICFile(filepath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage)))
			{
				return false;
			}
		}

		postProcessImage(scratchImage, metadata, flags, filepath, cacheFilepath);
	}

	createDesc(metadata, flags, textureDesc);

	return true;
}

static bool loadImageFromMemory(const void* data, uint32 size, image_format imageFormat, const fs::path& cachingFilepath, 
	uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc)
{
	if (flags & texture_load_flags_gen_mips_on_gpu)
	{
		flags &= ~texture_load_flags_gen_mips_on_cpu;
		flags |= texture_load_flags_allocate_full_mipchain;
	}


	fs::path extension = cachingFilepath.extension();

	fs::path cachedFilename = cachingFilepath;
	cachedFilename.replace_extension("." + std::to_string(flags) + ".cache.dds");

	fs::path cacheFilepath = L"asset_cache" / cachedFilename;

	bool fromCache = false;
	DirectX::TexMetadata metadata;

	if (!(flags & texture_load_flags_always_load_from_source))
	{
		// Look for cached.

		if (fs::exists(cacheFilepath))
		{
			fromCache = SUCCEEDED(DirectX::LoadFromDDSFile(cacheFilepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
		}
	}

	if (!fromCache)
	{
		if (flags & texture_load_flags_cache_to_dds)
		{
			std::cout << "Preprocessing in-memory texture'" << cachingFilepath.string() << "' for faster loading next time.";
#ifdef _DEBUG
			std::cout << " Consider running in a release build the first time.";
#endif
			std::cout << std::endl;
		}


		if (imageFormat == image_format_dds)
		{
			if (FAILED(DirectX::LoadFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, &metadata, scratchImage)))
			{
				return false;
			}
		}
		else if (imageFormat == image_format_hdr)
		{
			if (FAILED(DirectX::LoadFromHDRMemory(data, size, &metadata, scratchImage)))
			{
				return false;
			}
		}
		else if (imageFormat == image_format_tga)
		{
			if (FAILED(DirectX::LoadFromTGAMemory(data, size, &metadata, scratchImage)))
			{
				return false;
			}
		}
		else
		{
			assert(imageFormat == image_format_wic);

			if (FAILED(DirectX::LoadFromWICMemory(data, size, DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage)))
			{
				return false;
			}
		}

		postProcessImage(scratchImage, metadata, flags, cacheFilepath, cacheFilepath);
	}

	createDesc(metadata, flags, textureDesc);

	return true;
}

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

	if (flags & texture_load_flags_gen_mips_on_gpu)
	{
		dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		generateMipMapsOnGPU(cl, result);
		dxContext.executeCommandList(cl);
	}

	return result;
}

static ref<dx_texture> loadTextureInternal(const std::string& filename, uint32 flags)
{
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	if (!loadImageFromFile(filename, flags, scratchImage, textureDesc))
	{
		return nullptr;
	}

	return uploadImageToGPU(scratchImage, textureDesc, flags);
}

static ref<dx_texture> loadTextureFromMemoryInternal(const void* ptr, uint32 size, image_format imageFormat, const std::string& cacheFilename, uint32 flags)
{
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	if (!loadImageFromMemory(ptr, size, imageFormat, cacheFilename, flags, scratchImage, textureDesc))
	{
		return nullptr;
	}

	return uploadImageToGPU(scratchImage, textureDesc, flags);
}

static ref<dx_texture> loadVolumeTextureInternal(const std::string& dirname, uint32 flags)
{
	// No mip maps allowed for now!
	assert(!(flags & texture_load_flags_allocate_full_mipchain));
	assert(!(flags & texture_load_flags_gen_mips_on_cpu));
	assert(!(flags & texture_load_flags_gen_mips_on_gpu));

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

static std::unordered_map<std::string, weakref<dx_texture>> textureCache; // TODO: Pack flags into key.
static std::mutex mutex;

ref<dx_texture> loadTextureFromFile(const std::string& filename, uint32 flags)
{
	mutex.lock();

	const std::string& s = filename;
	
	auto sp = textureCache[s].lock();
	if (!sp)
	{
		textureCache[s] = sp = loadTextureInternal(s, flags);
	}

	mutex.unlock();
	return sp;
}

ref<dx_texture> loadTextureFromMemory(const void* ptr, uint32 size, image_format imageFormat, const std::string& cacheFilename, uint32 flags)
{
	mutex.lock();

	const std::string& s = cacheFilename;

	auto sp = textureCache[s].lock();
	if (!sp)
	{
		textureCache[s] = sp = loadTextureFromMemoryInternal(ptr, size, imageFormat, s, flags);
	}

	mutex.unlock();
	return sp;
}

ref<dx_texture> loadVolumeTextureFromDirectory(const std::string& dirname, uint32 flags)
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

void uploadTextureSubresourceData(ref<dx_texture> texture, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources)
{
	dx_command_list* cl = dxContext.getFreeCopyCommandList();
	cl->transitionBarrier(texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	UINT64 requiredSize = GetRequiredIntermediateSize(texture->resource.Get(), firstSubresource, numSubresources);

	dx_resource intermediateResource;
	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)
	));

	UpdateSubresources<128>(cl->commandList.Get(), texture->resource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);
	dxContext.retire(intermediateResource);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.
	//cl->transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(cl);
}

ref<dx_texture> createTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState)
{
	ref<dx_texture> result = make_ref<dx_texture>();

	result->requestedNumMipLevels = textureDesc.MipLevels;

	uint32 maxNumMipLevels = (uint32)log2(max(textureDesc.Width, textureDesc.Height)) + 1;
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
	checkResult(dxContext.device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		0,
		IID_PPV_ARGS(&result->resource)));


	result->numMipLevels = result->resource->GetDesc().MipLevels;

	result->format = textureDesc.Format;
	result->width = (uint32)textureDesc.Width;
	result->height = textureDesc.Height;
	result->depth = textureDesc.DepthOrArraySize;

	result->defaultSRV = {};
	result->defaultUAV = {};
	result->rtvHandles = {};
	result->dsvHandle = {};
	result->stencilSRV = {};

	result->initialState = initialState;


	// Upload.
	if (subresourceData)
	{
		uploadTextureSubresourceData(result, subresourceData, 0, numSubresources);
	}

	// SRV.
	if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		result->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createVolumeTextureSRV(result);
	}
	else if (textureDesc.DepthOrArraySize == 6)
	{
		result->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createCubemapSRV(result);
	}
	else
	{
		result->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureSRV(result);
	}

	// RTV.
	if (result->supportsRTV)
	{
		result->rtvHandles = dxContext.rtvAllocator.getFreeHandle().create2DTextureRTV(result);
	}

	// UAV.
	if (result->supportsUAV)
	{
		if (textureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			result->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createVolumeTextureUAV(result);
		}
		else if (textureDesc.DepthOrArraySize == 6)
		{
			result->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createCubemapUAV(result);
		}
		else
		{
			result->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureUAV(result);
		}
	}

	return result;
}

ref<dx_texture> createTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState)
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

		return createTexture(textureDesc, &subresource, 1, initialState);
	}
	else
	{
		return createTexture(textureDesc, 0, 0, initialState);
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

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		&optimizedClearValue,
		IID_PPV_ARGS(&result->resource)
	));

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
	result->supportsDSV = formatSupportsDSV(formatSupport);
	result->supportsUAV = false;
	result->supportsSRV = formatSupportsSRV(formatSupport);

	result->defaultSRV = {};
	result->defaultUAV = {};
	result->rtvHandles = {};
	result->dsvHandle = {};
	result->stencilSRV = {};

	result->initialState = initialState;

	assert(result->supportsDSV);

	result->dsvHandle = dxContext.dsvAllocator.getFreeHandle().create2DTextureDSV(result);
	if (arrayLength == 1)
	{
		result->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createDepthTextureSRV(result);

		if (isStencilFormat(format))
		{
			result->stencilSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createStencilTextureSRV(result);
		}
	}
	else
	{
		result->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createDepthTextureArraySRV(result);
	}

	return result;
}

ref<dx_texture> createCubeTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allocateMips, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState)
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

		return createTexture(textureDesc, subresources, 6, initialState);
	}
	else
	{
		return createTexture(textureDesc, 0, 0, initialState);
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
	name[min(arraysize(name) - 1, size)] = 0;

	return name;
}

void allocateMipUAVs(ref<dx_texture> texture)
{
	auto desc = texture->resource->GetDesc();
	assert(texture->supportsUAV);
	assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D); // Currently only supported for 2D textures.

	uint32 mipLevels = desc.MipLevels;
	for (uint32 i = 1; i < mipLevels; ++i)
	{
		dx_cpu_descriptor_handle h = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureUAV(texture, i);
		texture->mipUAVs.push_back(h);
	}
}

static void retire(dx_resource resource, dx_cpu_descriptor_handle srv, dx_cpu_descriptor_handle uav, dx_cpu_descriptor_handle stencil, dx_rtv_descriptor_handle rtv, dx_dsv_descriptor_handle dsv,
	std::vector<dx_cpu_descriptor_handle>&& mipUAVs)
{
	texture_grave grave;
	grave.resource = resource;
	grave.srv = srv;
	grave.uav = uav;
	grave.stencil = stencil;
	grave.rtv = rtv;
	grave.dsv = dsv;
	grave.mipUAVs = std::move(mipUAVs);
	dxContext.retire(std::move(grave));
}

dx_texture::~dx_texture()
{
	retire(resource, defaultSRV, defaultUAV, stencilSRV, rtvHandles, dsvHandle, std::move(mipUAVs));
}

void resizeTexture(ref<dx_texture> texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState)
{
	wchar name[128];
	uint32 size = sizeof(name);
	texture->resource->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);
	name[min(arraysize(name) - 1, size)] = 0;

	bool hasMipUAVs = texture->mipUAVs.size() > 0;

	retire(texture->resource, texture->defaultSRV, texture->defaultUAV, texture->stencilSRV, texture->rtvHandles, texture->dsvHandle, std::move(texture->mipUAVs));

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

	uint32 maxNumMipLevels = (uint32)log2(max(newWidth, newHeight)) + 1;
	desc.MipLevels = min(maxNumMipLevels, texture->requestedNumMipLevels);

	desc.Width = newWidth;
	desc.Height = newHeight;
	texture->width = newWidth;
	texture->height = newHeight;


	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(&texture->resource)
	));

	texture->numMipLevels = texture->resource->GetDesc().MipLevels;

	// RTV.
	if (texture->supportsRTV)
	{
		texture->rtvHandles = dxContext.rtvAllocator.getFreeHandle().create2DTextureRTV(texture);
	}

	// DSV & SRV.
	if (texture->supportsDSV)
	{
		texture->dsvHandle = dxContext.dsvAllocator.getFreeHandle().create2DTextureDSV(texture);
		if (texture->depth == 1)
		{
			texture->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createDepthTextureSRV(texture);
		}
		else
		{
			texture->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createDepthTextureArraySRV(texture);
		}

		if (isStencilFormat(texture->format))
		{
			texture->stencilSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createStencilTextureSRV(texture);
		}
	}
	else
	{
		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			texture->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createVolumeTextureSRV(texture);
		}
		else if (texture->depth == 6)
		{
			texture->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createCubemapSRV(texture);
		}
		else
		{
			texture->defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureSRV(texture);
		}
	}

	// UAV.
	if (texture->supportsUAV)
	{
		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			texture->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createVolumeTextureUAV(texture);
		}
		else if (texture->depth == 6)
		{
			texture->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createCubemapUAV(texture);
		}
		else
		{
			texture->defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureUAV(texture);
		}
	}

	if (hasMipUAVs)
	{
		allocateMipUAVs(texture);
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
		name[min(arraysize(name) - 1, size)] = 0;

		if (srv.cpuHandle.ptr)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(srv);
		}
		if (uav.cpuHandle.ptr)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(uav);
		}
		if (stencil.cpuHandle.ptr)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(stencil);
		}
		if (rtv.cpuHandle.ptr)
		{
			dxContext.rtvAllocator.freeHandle(rtv);
		}
		if (dsv.cpuHandle.ptr)
		{
			dxContext.dsvAllocator.freeHandle(dsv);
		}

		for (dx_cpu_descriptor_handle h : mipUAVs)
		{
			dxContext.descriptorAllocatorCPU.freeHandle(h);
		}
	}
}


