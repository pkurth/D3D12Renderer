#include "pch.h"
#include "texture_preprocessing.h"
#include "dx/dx_context.h"
#include "core/math.h"
#include "dx/dx_command_list.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_barrier_batcher.h"



static dx_pipeline mipmapPipeline;
static dx_pipeline equirectangularToCubemapPipeline;

static dx_pipeline texturedSkyToIrradiancePipeline;
static dx_pipeline texturedSkyToIrradianceSHPipeline;
static dx_pipeline texturedSkyToPrefilteredRadiancePipeline;

static dx_pipeline proceduralSkyToIrradiancePipeline;
//static dx_pipeline proceduralSkyToPrefilteredRadiancePipeline;

static dx_pipeline integrateBRDFPipeline;



struct mipmap_cb
{
	uint32 srcMipLevel;				// Texture level of source mip
	uint32 numMipLevelsToGenerate;	// The shader can generate up to 4 mips at once.
	uint32 srcDimensionFlags;		// Flags specifying whether width and height are even or odd (see above).
	uint32 isSRGB;					// Must apply gamma correction to sRGB textures.
	vec2 texelSize;					// 1.0 / OutMip1.Dimensions
};

struct equirectangular_to_cubemap_cb
{
	uint32 cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint32 firstMip;				// The first mip level to generate.
	uint32 numMipLevelsToGenerate;	// The number of mips to generate.
	uint32 isSRGB;
};

struct textured_sky_to_irradiance_cb
{
	uint32 irradianceMapSize;
};

struct textured_sky_to_irradiance_sh_cb
{
	uint32 mipLevel;
};

struct procedural_sky_to_irradiance_cb
{
	vec3 sunDirection;
	uint32 irradianceMapSize;
};

struct textured_sky_to_prefiltered_radiance_cb
{
	uint32 cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint32 firstMip;				// The first mip level to generate.
	uint32 numMipLevelsToGenerate;	// The number of mips to generate.
	uint32 totalNumMipLevels;
};

struct integrate_brdf_cb
{
	uint32 textureDim;
};

void initializeTexturePreprocessing()
{
	mipmapPipeline = createReloadablePipeline("generate_mips_cs");
	equirectangularToCubemapPipeline = createReloadablePipeline("equirectangular_to_cubemap_cs");

	texturedSkyToIrradiancePipeline = createReloadablePipeline("textured_sky_to_irradiance_cs");
	texturedSkyToIrradianceSHPipeline = createReloadablePipeline("textured_sky_to_irradiance_sh_cs");
	texturedSkyToPrefilteredRadiancePipeline = createReloadablePipeline("textured_sky_to_prefiltered_radiance_cs");

	proceduralSkyToIrradiancePipeline = createReloadablePipeline("procedural_sky_to_irradiance_cs");
	//proceduralSkyToPrefilteredRadiancePipeline = createReloadablePipeline("procedural_sky_to_prefiltered_radiance_cs");

	integrateBRDFPipeline = createReloadablePipeline("integrate_brdf_cs");
}

void generateMipMapsOnGPU(dx_command_list* cl, ref<dx_texture>& texture)
{
	dx_resource resource = texture->resource;
	if (!resource)
	{
		return;
	}

	D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

	uint32 numMips = resourceDesc.MipLevels;
	if (numMips == 1)
	{
		return;
	}

	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		resourceDesc.DepthOrArraySize != 1 ||
		resourceDesc.SampleDesc.Count > 1)
	{
		std::cerr << "GenerateMips is only supported for non-multi-sampled 2D Textures.\n";
		return;
	}

	dx_resource uavResource = resource;
	dx_resource aliasResource; // In case the format of our texture does not support UAVs.

	if (!texture->supportsUAV)
	{
		D3D12_RESOURCE_DESC aliasDesc = resourceDesc;
		// Placed resources can't be render targets or depth-stencil views.
		aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		// Describe a UAV compatible resource that is used to perform
		// mipmapping of the original texture.
		D3D12_RESOURCE_DESC uavDesc = aliasDesc;   // The flags for the UAV description must match that of the alias description.
		uavDesc.Format = getUAVCompatibleFormat(resourceDesc.Format);

		D3D12_RESOURCE_DESC resourceDescs[] = {
			aliasDesc,
			uavDesc
		};

		// Create a heap that is large enough to store a copy of the original resource.
		D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = dxContext.device->GetResourceAllocationInfo(0, arraysize(resourceDescs), resourceDescs);

		D3D12_HEAP_DESC heapDesc = {};
		heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
		heapDesc.Alignment = allocationInfo.Alignment;
		heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
		heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

		dx_heap heap;
		checkResult(dxContext.device->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap)));
		dxContext.retire(heap);

		checkResult(dxContext.device->CreatePlacedResource(
			heap.Get(),
			0,
			&aliasDesc,
			D3D12_RESOURCE_STATE_COMMON,
			0,
			IID_PPV_ARGS(&aliasResource)
		));

		SET_NAME(aliasResource, "Alias resource for mip map generation");

		dxContext.retire(aliasResource);

		checkResult(dxContext.device->CreatePlacedResource(
			heap.Get(),
			0,
			&uavDesc,
			D3D12_RESOURCE_STATE_COMMON,
			0,
			IID_PPV_ARGS(&uavResource)
		));

		SET_NAME(uavResource, "UAV resource for mip map generation");

		dxContext.retire(uavResource);

		cl->aliasingBarrier(0, aliasResource);

		// Copy the original resource to the alias resource.
		cl->copyResource(resource, aliasResource);

		// Add an aliasing barrier for the UAV compatible resource.
		cl->aliasingBarrier(aliasResource, uavResource);
	}

	bool isSRGB = isSRGBFormat(resourceDesc.Format);
	cl->setPipelineState(*mipmapPipeline.pipeline);
	cl->setComputeRootSignature(*mipmapPipeline.rootSignature);

	mipmap_cb cb;
	cb.isSRGB = isSRGB;

	resourceDesc = uavResource->GetDesc();

	DXGI_FORMAT overrideFormat = isSRGB ? getSRGBFormat(resourceDesc.Format) : resourceDesc.Format;

	ref<dx_texture> tmpTexture = make_ref<dx_texture>();
	tmpTexture->resource = uavResource;

	uint32 numSrcMipLevels = resourceDesc.MipLevels - 1;
	uint32 numDstMipLevels = resourceDesc.MipLevels - 1;
	uint32 numDescriptors = numSrcMipLevels + numDstMipLevels;

	dx_descriptor_range descriptors = dxContext.frameDescriptorAllocator.allocateContiguousDescriptorRange(numDescriptors);

	dx_double_descriptor_handle srvOffset;
	for (uint32 i = 0; i < numSrcMipLevels; ++i)
	{
		dx_double_descriptor_handle h = descriptors.pushHandle();
		h.create2DTextureSRV(tmpTexture, { i, 1 }, overrideFormat);
		if (i == 0)
		{
			srvOffset = h;
		}
	}

	dx_double_descriptor_handle uavOffset;
	for (uint32 i = 0; i < numDstMipLevels; ++i)
	{
		uint32 mip = i + 1;
		dx_double_descriptor_handle h = descriptors.pushHandle();
		h.create2DTextureUAV(tmpTexture, mip);
		if (i == 0)
		{
			uavOffset = h;
		}
	}

	cl->setDescriptorHeap(descriptors);

	for (uint32 srcMip = 0; srcMip < resourceDesc.MipLevels - 1u; )
	{
		uint64 srcWidth = resourceDesc.Width >> srcMip;
		uint32 srcHeight = resourceDesc.Height >> srcMip;
		uint32 dstWidth = (uint32)(srcWidth >> 1);
		uint32 dstHeight = srcHeight >> 1;

		// 0b00(0): Both width and height are even.
		// 0b01(1): Width is odd, height is even.
		// 0b10(2): Width is even, height is odd.
		// 0b11(3): Both width and height are odd.
		cb.srcDimensionFlags = (srcHeight & 1) << 1 | (srcWidth & 1);

		DWORD mipCount;

		// The number of times we can half the size of the texture and get
		// exactly a 50% reduction in size.
		// A 1 bit in the width or height indicates an odd dimension.
		// The case where either the width or the height is exactly 1 is handled
		// as a special case (as the dimension does not require reduction).
		_BitScanForward(&mipCount, (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));

		// Maximum number of mips to generate is 4.
		mipCount = min(4, (int)mipCount + 1);
		// Clamp to total number of mips left over.
		mipCount = (srcMip + mipCount) >= resourceDesc.MipLevels ?
			resourceDesc.MipLevels - srcMip - 1 : mipCount;

		// Dimensions should not reduce to 0.
		// This can happen if the width and height are not the same.
		dstWidth = max(1u, dstWidth);
		dstHeight = max(1u, dstHeight);

		cb.srcMipLevel = srcMip;
		cb.numMipLevelsToGenerate = mipCount;
		cb.texelSize.x = 1.f / (float)dstWidth;
		cb.texelSize.y = 1.f / (float)dstHeight;

		cl->setCompute32BitConstants(0, cb);

		cl->setComputeDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(srvOffset.gpuHandle, srcMip, descriptors.descriptorHandleIncrementSize));
		cl->setComputeDescriptorTable(2, CD3DX12_GPU_DESCRIPTOR_HANDLE(uavOffset.gpuHandle, srcMip, descriptors.descriptorHandleIncrementSize));

		cl->dispatch(bucketize(dstWidth, 8), bucketize(dstHeight, 8));

		cl->uavBarrier(uavResource);

		srcMip += mipCount;
	}

	if (aliasResource)
	{
		cl->aliasingBarrier(uavResource, aliasResource);
		// Copy the alias resource back to the original resource.
		barrier_batcher(cl)
			.transition(aliasResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE)
			.transition(resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		cl->copyResource(aliasResource, resource);
		cl->transitionBarrier(resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	}

	cl->resetToDynamicDescriptorHeap();
}

ref<dx_texture> equirectangularToCubemap(dx_command_list* cl, const ref<dx_texture>& equirectangular, uint32 resolution, uint32 numMips, DXGI_FORMAT format)
{
	ASSERT(equirectangular->resource);

	CD3DX12_RESOURCE_DESC cubemapDesc(equirectangular->resource->GetDesc());
	cubemapDesc.Width = cubemapDesc.Height = resolution;
	cubemapDesc.DepthOrArraySize = 6;
	cubemapDesc.MipLevels = numMips;
	if (format != DXGI_FORMAT_UNKNOWN)
	{
		cubemapDesc.Format = format;
	}
	if (isUAVCompatibleFormat(cubemapDesc.Format))
	{
		cubemapDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	ref<dx_texture> cubemap = createTexture(cubemapDesc, 0, 0);

	cubemapDesc = CD3DX12_RESOURCE_DESC(cubemap->resource->GetDesc());
	numMips = cubemapDesc.MipLevels;

	dx_resource cubemapResource = cubemap->resource;
	SET_NAME(cubemapResource, "Cubemap");
	dx_resource stagingResource = cubemapResource;

	ref<dx_texture> stagingTexture = make_ref<dx_texture>();
	stagingTexture->resource = cubemapResource;


	if ((cubemapDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		CD3DX12_RESOURCE_DESC stagingDesc = cubemapDesc;
		stagingDesc.Format = getUAVCompatibleFormat(cubemapDesc.Format);
		stagingDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		auto heapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		checkResult(dxContext.device->CreateCommittedResource(
			&heapDesc,
			D3D12_HEAP_FLAG_NONE,
			&stagingDesc,
			D3D12_RESOURCE_STATE_COMMON,
			0,
			IID_PPV_ARGS(&stagingResource)
		));

		SET_NAME(stagingResource, "Staging resource for equirectangular to cubemap");

		stagingTexture->resource = stagingResource;
	}

	cl->setPipelineState(*equirectangularToCubemapPipeline.pipeline);
	cl->setComputeRootSignature(*equirectangularToCubemapPipeline.rootSignature);

	bool isSRGB = isSRGBFormat(cubemapDesc.Format);

	equirectangular_to_cubemap_cb equirectangularToCubemapCB;
	equirectangularToCubemapCB.isSRGB = isSRGB;


	dx_descriptor_range descriptors = dxContext.frameDescriptorAllocator.allocateContiguousDescriptorRange(numMips + 1);

	dx_double_descriptor_handle srvOffset = descriptors.pushHandle();
	srvOffset.create2DTextureSRV(equirectangular);

	cl->setDescriptorHeap(descriptors);
	cl->setComputeDescriptorTable(1, srvOffset);


	for (uint32 mipSlice = 0; mipSlice < numMips; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32 numMips = min(5u, cubemapDesc.MipLevels - mipSlice);

		equirectangularToCubemapCB.firstMip = mipSlice;
		equirectangularToCubemapCB.cubemapSize = max((uint32)cubemapDesc.Width, cubemapDesc.Height) >> mipSlice;
		equirectangularToCubemapCB.numMipLevelsToGenerate = numMips;

		cl->setCompute32BitConstants(0, equirectangularToCubemapCB);


		for (uint32 mip = 0; mip < numMips; ++mip)
		{
			dx_double_descriptor_handle h = descriptors.pushHandle();
			h.create2DTextureArrayUAV(stagingTexture, mipSlice + mip, getUAVCompatibleFormat(cubemapDesc.Format));
			if (mip == 0)
			{
				cl->setComputeDescriptorTable(2, h.gpuHandle);
			}
		}

		cl->dispatch(bucketize(equirectangularToCubemapCB.cubemapSize, 16), bucketize(equirectangularToCubemapCB.cubemapSize, 16), 6);

		mipSlice += numMips;
	}

	cl->uavBarrier(stagingResource);

	if (stagingResource != cubemapResource)
	{
		cl->copyResource(stagingTexture->resource, cubemap->resource);
		cl->transitionBarrier(cubemap->resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	}

	return cubemap;
}

void texturedSkyToIrradiance(dx_command_list* cl, const ref<dx_texture>& sky, ref<dx_texture>& outIrradiance)
{
	ASSERT(sky && sky->resource);
	ASSERT(outIrradiance && outIrradiance->resource);

	ASSERT(outIrradiance->supportsUAV);
	ASSERT(outIrradiance->depth == 6);
	ASSERT(outIrradiance->width == outIrradiance->height);

	cl->setPipelineState(*texturedSkyToIrradiancePipeline.pipeline);
	cl->setComputeRootSignature(*texturedSkyToIrradiancePipeline.rootSignature);

	textured_sky_to_irradiance_cb cb;
	cb.irradianceMapSize = outIrradiance->width;

	cl->setCompute32BitConstants(0, cb);
	cl->setDescriptorHeapSRV(1, 0, sky);
	cl->setDescriptorHeapUAV(1, 1, outIrradiance);

	cl->dispatch(bucketize(cb.irradianceMapSize, 16), bucketize(cb.irradianceMapSize, 16), 6);

	cl->uavBarrier(outIrradiance);
}

void texturedSkyToPrefilteredRadiance(dx_command_list* cl, const ref<dx_texture>& sky, ref<dx_texture>& outPrefilteredRadiance)
{
	ASSERT(sky && sky->resource);
	ASSERT(outPrefilteredRadiance && outPrefilteredRadiance->resource);

	ASSERT(outPrefilteredRadiance->supportsUAV);
	ASSERT(outPrefilteredRadiance->depth == 6);
	ASSERT(outPrefilteredRadiance->width == outPrefilteredRadiance->height);

	cl->setPipelineState(*texturedSkyToPrefilteredRadiancePipeline.pipeline);
	cl->setComputeRootSignature(*texturedSkyToPrefilteredRadiancePipeline.rootSignature);

	textured_sky_to_prefiltered_radiance_cb cb;
	cb.totalNumMipLevels = outPrefilteredRadiance->numMipLevels;

	cl->setDescriptorHeapSRV(1, 0, sky);

	uint32 dimensions = outPrefilteredRadiance->width;

	for (uint32 mipSlice = 0; mipSlice < outPrefilteredRadiance->numMipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32 numMips = min(5u, outPrefilteredRadiance->numMipLevels - mipSlice);

		cb.firstMip = mipSlice;
		cb.cubemapSize = dimensions >> mipSlice;
		cb.numMipLevelsToGenerate = numMips;

		cl->setCompute32BitConstants(0, cb);

		for (uint32 mip = 0; mip < numMips; ++mip)
		{
			cl->setDescriptorHeapUAV(1, 1 + mip, outPrefilteredRadiance->uavAt(mipSlice + mip));
		}

		cl->dispatch(bucketize(cb.cubemapSize, 16), bucketize(cb.cubemapSize, 16), 6);

		mipSlice += numMips;
	}

	cl->uavBarrier(outPrefilteredRadiance);
}

void proceduralSkyToIrradiance(dx_command_list* cl, vec3 sunDirection, ref<dx_texture>& outIrradiance)
{
	ASSERT(outIrradiance && outIrradiance->resource);

	ASSERT(outIrradiance->supportsUAV);
	ASSERT(outIrradiance->depth == 6);
	ASSERT(outIrradiance->width == outIrradiance->height);

	cl->setPipelineState(*proceduralSkyToIrradiancePipeline.pipeline);
	cl->setComputeRootSignature(*proceduralSkyToIrradiancePipeline.rootSignature);

	procedural_sky_to_irradiance_cb cb;
	cb.sunDirection = -sunDirection;
	cb.irradianceMapSize = outIrradiance->width;

	cl->setCompute32BitConstants(0, cb);
	cl->setDescriptorHeapUAV(1, 0, outIrradiance);

	cl->dispatch(bucketize(cb.irradianceMapSize, 16), bucketize(cb.irradianceMapSize, 16), 6);

	cl->uavBarrier(outIrradiance);
}

#if 0
void proceduralSkyToPrefilteredRadiance(dx_command_list* cl, vec3 sunDirection, ref<dx_texture>& outPrefilteredRadiance)
{
	ASSERT(outPrefilteredRadiance && outPrefilteredRadiance->resource);

	ASSERT(outPrefilteredRadiance->supportsUAV);
	ASSERT(outPrefilteredRadiance->depth == 6);
	ASSERT(outPrefilteredRadiance->width == outPrefilteredRadiance->height);

	cl->setPipelineState(*proceduralSkyToPrefilteredRadiancePipeline.pipeline);
	cl->setComputeRootSignature(*proceduralSkyToPrefilteredRadiancePipeline.rootSignature);

	textured_sky_to_prefiltered_radiance_cb cb;
	cb.totalNumMipLevels = outPrefilteredRadiance->numMipLevels;

	uint32 dimensions = outPrefilteredRadiance->width;

	for (uint32 mipSlice = 0; mipSlice < outPrefilteredRadiance->numMipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32 numMips = min(5u, outPrefilteredRadiance->numMipLevels - mipSlice);

		cb.firstMip = mipSlice;
		cb.cubemapSize = dimensions >> mipSlice;
		cb.numMipLevelsToGenerate = numMips;

		cl->setCompute32BitConstants(0, cb);

		for (uint32 mip = 0; mip < numMips; ++mip)
		{
			cl->setDescriptorHeapUAV(1, mip, outPrefilteredRadiance->uavAt(mipSlice + mip));
		}

		cl->dispatch(bucketize(cb.cubemapSize, 16), bucketize(cb.cubemapSize, 16), 6);

		mipSlice += numMips;
	}

	cl->uavBarrier(outPrefilteredRadiance);
}
#endif

ref<dx_texture> integrateBRDF(dx_command_list* cl, uint32 resolution)
{
	CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R16G16_FLOAT,
		resolution, resolution, 1, 1);

	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ref<dx_texture> brdf = createTexture(desc, 0, 0);
	desc = CD3DX12_RESOURCE_DESC(brdf->resource->GetDesc());

	// TODO: Technically R16G16 is not guaranteed to be UAV compatible.
	// If we ever run on hardware, which does not support this, we need to find a solution.
	// https://docs.microsoft.com/en-us/windows/win32/direct3d11/typed-unordered-access-view-loads

	SET_NAME(brdf->resource, "BRDF");

	cl->setPipelineState(*integrateBRDFPipeline.pipeline);
	cl->setComputeRootSignature(*integrateBRDFPipeline.rootSignature);

	integrate_brdf_cb integrateBrdfCB;
	integrateBrdfCB.textureDim = resolution;

	cl->setCompute32BitConstants(0, integrateBrdfCB);

	cl->resetToDynamicDescriptorHeap();
	cl->setDescriptorHeapUAV(1, 0, brdf);

	cl->dispatch(bucketize(resolution, 16), bucketize(resolution, 16), 1);

	cl->uavBarrier(brdf);

	return brdf;
}

void texturedSkyToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment, ref<dx_buffer> shBuffer, uint32 shIndex)
{
	cl->setPipelineState(*texturedSkyToIrradianceSHPipeline.pipeline);
	cl->setComputeRootSignature(*texturedSkyToIrradianceSHPipeline.rootSignature);

	ASSERT(environment->width == environment->height);
	ASSERT(environment->width >= 64);
	ASSERT(environment->depth == 6);
	ASSERT(shBuffer->elementCount > shIndex);

	textured_sky_to_irradiance_sh_cb cb;
	cb.mipLevel = environment->numMipLevels == 1 ? 0 : (environment->numMipLevels - 6);

	cl->setCompute32BitConstants(0, cb);
	cl->setDescriptorHeapSRV(1, 0, environment);
	cl->setRootComputeUAV(2, shBuffer->gpuVirtualAddress + sizeof(spherical_harmonics) * shIndex);

	cl->dispatch(1, 1);
	cl->uavBarrier(shBuffer);
}

ref<dx_buffer> texturedSkyToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment)
{
	ref<dx_buffer> buffer = createBuffer(sizeof(spherical_harmonics), 1, 0, true);
	texturedSkyToIrradianceSH(cl, environment, buffer, 0);
	return buffer;
}

