#include "pch.h"
#include "image.h"
#include "log.h"
#include "memory.h"

#define NANOSVG_IMPLEMENTATION
#include <nanosvg/nanosvg.h>

#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>



bool isImageExtension(const fs::path& extension)
{
	return extension == ".jpg" ||
		extension == ".png" ||
		extension == ".tga" ||
		extension == ".hdr" ||
		extension == ".dds";
}

bool isImageExtension(const std::string& extension)
{
	return extension == ".jpg" ||
		extension == ".png" ||
		extension == ".tga" ||
		extension == ".hdr" ||
		extension == ".dds";
}

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

static bool tryLoadFromCache(const fs::path& filepath, uint32 flags, fs::path& cacheFilepath, DirectX::ScratchImage& scratchImage, DirectX::TexMetadata& metadata)
{
	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension("." + std::to_string(flags) + ".cache.dds");

	cacheFilepath = L"asset_cache" / cachedFilename;

	bool fromCache = false;

	if (!(flags & image_load_flags_always_load_from_source))
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

	return fromCache;
}

static void createDesc(DirectX::TexMetadata& metadata, uint32 flags, D3D12_RESOURCE_DESC& textureDesc)
{
	if (flags & image_load_flags_allocate_full_mipchain)
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

static void postProcessImage(DirectX::ScratchImage& scratchImage, DirectX::TexMetadata& metadata, uint32 flags, const fs::path& filepath, const fs::path& cacheFilepath)
{
	if (flags & image_load_flags_noncolor)
	{
		metadata.format = makeLinear(metadata.format);
	}
	else
	{
		metadata.format = makeSRGB(metadata.format);
	}

	scratchImage.OverrideFormat(metadata.format);

	if (flags & image_load_flags_gen_mips_on_cpu && !DirectX::IsCompressed(metadata.format))
	{
		DirectX::ScratchImage mipchainImage;

		checkResult(DirectX::GenerateMipMaps(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::TEX_FILTER_DEFAULT, 0, mipchainImage));
		scratchImage = std::move(mipchainImage);
		metadata = scratchImage.GetMetadata();
	}
	else
	{
		metadata.mipLevels = max(1u, (uint32)metadata.mipLevels);
	}

	if (flags & image_load_flags_premultiply_alpha)
	{
		DirectX::ScratchImage premultipliedAlphaImage;

		checkResult(DirectX::PremultiplyAlpha(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::TEX_PMALPHA_DEFAULT, premultipliedAlphaImage));
		scratchImage = std::move(premultipliedAlphaImage);
		metadata = scratchImage.GetMetadata();
	}

	if (DirectX::IsCompressed(metadata.format) && metadata.width == 1 && metadata.height == 1)
	{
		assert(metadata.mipLevels == 1);
		assert(scratchImage.GetImageCount() == 1);

		DirectX::TexMetadata scaledMetadata = metadata;
		scaledMetadata.width = 4;
		scaledMetadata.height = 4;
		scaledMetadata.mipLevels = 1;

		DirectX::ScratchImage scaledImage;
		checkResult(scaledImage.Initialize(scaledMetadata));

		memcpy(scaledImage.GetImage(0, 0, 0)->pixels, scratchImage.GetImage(0, 0, 0)->pixels, scratchImage.GetPixelsSize());

		scratchImage = std::move(scaledImage);
		metadata = scratchImage.GetMetadata();
	}

	if (flags & image_load_flags_compress && !DirectX::IsCompressed(metadata.format))
	{
		if (metadata.width % 4 == 0 && metadata.height % 4 == 0)
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
		else
		{
			LOG_ERROR("Cannot compress texture '%ws', since its dimensions are not a multiple of 4", filepath.c_str());
			std::cerr << "Cannot compress texture '" << filepath << "', since its dimensions are not a multiple of 4.\n";
		}
	}

	if (flags & image_load_flags_cache_to_dds)
	{
		fs::create_directories(cacheFilepath.parent_path());
		checkResult(DirectX::SaveToDDSFile(scratchImage.GetImages(), scratchImage.GetImageCount(), metadata, DirectX::DDS_FLAGS_NONE, cacheFilepath.c_str()));
	}
}

bool loadImageFromMemory(const void* data, uint32 size, image_format imageFormat, const fs::path& cachingFilepath,
	uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc)
{
	if (flags & image_load_flags_gen_mips_on_gpu)
	{
		flags &= ~image_load_flags_gen_mips_on_cpu;
		flags |= image_load_flags_allocate_full_mipchain;
	}


	fs::path cachedFilename = cachingFilepath.string() + std::to_string(flags) + ".cache.dds";
	fs::path cacheFilepath = L"asset_cache" / cachedFilename;

	bool fromCache = false;
	DirectX::TexMetadata metadata;

	if (!(flags & image_load_flags_always_load_from_source))
	{
		// Look for cached.

		if (fs::exists(cacheFilepath))
		{
			fromCache = SUCCEEDED(DirectX::LoadFromDDSFile(cacheFilepath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage));
		}
	}

	if (!fromCache)
	{
		if (flags & image_load_flags_cache_to_dds)
		{
			LOG_MESSAGE("Preprocessing in-memory texture '%ws' for faster loading next time", cachingFilepath.c_str());
			std::cout << "Preprocessing in-memory texture '" << cachingFilepath.string() << "' for faster loading next time.";
#ifdef _DEBUG
			std::cout << " Consider running in a release build the first time.";
#endif
			std::cout << '\n';
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

bool loadSVGFromFile(const fs::path& filepath, uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc)
{
	fs::path cacheFilepath;
	DirectX::TexMetadata metadata;
	bool fromCache = tryLoadFromCache(filepath, flags, cacheFilepath, scratchImage, metadata);

	if (!fromCache)
	{
		if (!fs::exists(filepath))
		{
			LOG_WARNING("Could not find file '%ws'", filepath.c_str());
			std::cerr << "Could not find file '" << filepath.string() << "'.\n";
			return false;
		}

		if (flags & image_load_flags_cache_to_dds)
		{
			LOG_MESSAGE("Preprocessing asset '%ws' for faster loading next time", filepath.c_str());
			std::cout << "Preprocessing asset '" << filepath.string() << "' for faster loading next time.";
#ifdef _DEBUG
			std::cout << " Consider running in a release build the first time.";
#endif
			std::cout << '\n';
		}

		NSVGimage* svg = nsvgParseFromFile(filepath.string().c_str(), "px", 96);
		uint32 width = (uint32)ceil(svg->width);
		uint32 height = (uint32)ceil(svg->height);

		uint8* rawImage = new uint8[width * height * 4];

		NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
		nsvgRasterize(rasterizer, svg, 0, 0, 1, rawImage, width, height, width * 4);
		nsvgDeleteRasterizer(rasterizer);

		nsvgDelete(svg);


		DirectX::Image dxImage = { width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width * 4, width * height * 4, rawImage };
		scratchImage.InitializeFromImage(dxImage);
		metadata = scratchImage.GetMetadata();

		postProcessImage(scratchImage, metadata, flags, filepath, cacheFilepath);

		delete[] rawImage;
	}

	createDesc(metadata, flags, textureDesc);

	return true;
}

bool loadImageFromFile(const fs::path& filepath, uint32 flags, DirectX::ScratchImage& scratchImage, D3D12_RESOURCE_DESC& textureDesc)
{
	fs::path cacheFilepath;
	DirectX::TexMetadata metadata;
	bool fromCache = tryLoadFromCache(filepath, flags, cacheFilepath, scratchImage, metadata);

	if (!fromCache)
	{
		if (!fs::exists(filepath))
		{
			LOG_WARNING("Could not find file '%ws'", filepath.c_str());
			std::cerr << "Could not find file '" << filepath.string() << "'.\n";
			return false;
		}

		if (flags & image_load_flags_cache_to_dds)
		{
			LOG_MESSAGE("Preprocessing asset '%ws' for faster loading next time", filepath.c_str());
			std::cout << "Preprocessing asset '" << filepath.string() << "' for faster loading next time.";
#ifdef _DEBUG
			std::cout << " Consider running in a release build the first time.";
#endif
			std::cout << '\n';
		}

		fs::path extension = filepath.extension();

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


