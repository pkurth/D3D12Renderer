#include "pch.h"
#include "texture.h"
#include "dx_context.h"
#include "texture_preprocessing.h"

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

static bool loadImageFromFile(const char* filepathRaw, uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc)
{
	if (flags & texture_load_flags_gen_mips_on_gpu)
	{
		flags &= ~texture_load_flags_gen_mips_on_cpu;
		flags |= texture_load_flags_allocate_full_mipchain;
	}


	fs::path filepath = filepathRaw;
	fs::path extension = filepath.extension();

	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension("." + std::to_string(flags) + ".cache.dds");

	fs::path cacheFilepath = L"bin_cache" / cachedFilename;

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
		if (extension == ".dds")
		{
			checkResult(DirectX::LoadFromDDSFile(filepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
		}
		else if (extension == ".hdr")
		{
			checkResult(DirectX::LoadFromHDRFile(filepath.c_str(), &metadata, scratchImage));
		}
		else if (extension == ".tga")
		{
			checkResult(DirectX::LoadFromTGAFile(filepath.c_str(), &metadata, scratchImage));
		}
		else
		{
			checkResult(DirectX::LoadFromWICFile(filepath.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
		}

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
			DirectX::ScratchImage compressedImage;

			DirectX::TEX_COMPRESS_FLAGS compressFlags = DirectX::TEX_COMPRESS_PARALLEL;
			DXGI_FORMAT compressedFormat = DirectX::IsSRGB(metadata.format) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;

			/*if (metadata.format == DXGI_FORMAT_R16G16B16A16_FLOAT)
			{
				compressedFormat = DXGI_FORMAT_BC6H_UF16;
			}*/

			checkResult(DirectX::Compress(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata,
				compressedFormat, compressFlags, DirectX::TEX_THRESHOLD_DEFAULT, compressedImage));
			scratchImage = std::move(compressedImage);
			metadata = scratchImage.GetMetadata();
		}

		if (flags & texture_load_flags_cache_to_dds)
		{
			fs::create_directories(cacheFilepath.parent_path());
			checkResult(DirectX::SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::DDS_FLAGS_NONE, cacheFilepath.c_str()));
		}
	}

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

	return true;
}

static ref<dx_texture> loadTextureInternal(const char* filename, uint32 flags)
{
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC textureDesc;

	loadImageFromFile(filename, flags, scratchImage, textureDesc);

	const DirectX::Image* images = scratchImage.GetImages();
	uint32 numImages = (uint32)scratchImage.GetImageCount();

	D3D12_SUBRESOURCE_DATA subresources[64];
	for (uint32 i = 0; i < numImages; ++i)
	{
		D3D12_SUBRESOURCE_DATA& subresource = subresources[i];
		subresource.RowPitch = images[i].rowPitch;
		subresource.SlicePitch = images[i].slicePitch;
		subresource.pData = images[i].pixels;
	}

	ref<dx_texture> result = createTexture(textureDesc, subresources, numImages);

	if (flags & texture_load_flags_gen_mips_on_gpu)
	{
		dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		generateMipMapsOnGPU(cl, result);
		dxContext.executeCommandList(cl);
	}

	return result;
}

ref<dx_texture> loadTextureFromFile(const char* filename, uint32 flags)
{
	ref<dx_texture> result = loadTextureInternal(filename, flags);
	// TODO: Cache.
	return result;
}
