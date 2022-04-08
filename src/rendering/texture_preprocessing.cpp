#include "pch.h"
#include "texture_preprocessing.h"
#include "dx/dx_context.h"
#include "core/math.h"
#include "dx/dx_command_list.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_barrier_batcher.h"



static dx_pipeline mipmapPipeline;
static dx_pipeline equirectangularToCubemapPipeline;
static dx_pipeline cubemapToIrradiancePipeline;
static dx_pipeline cubemapToIrradianceSHPipeline;
static dx_pipeline prefilterEnvironmentPipeline;
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

struct cubemap_to_irradiance_cb
{
	uint32 irradianceMapSize;
};

struct cubemap_to_irradiance_sh_cb
{
	uint32 mipLevel;
};

struct prefilter_environment_cb
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
	cubemapToIrradiancePipeline = createReloadablePipeline("cubemap_to_irradiance_cs");
	cubemapToIrradianceSHPipeline = createReloadablePipeline("cubemap_to_irradiance_sh_cs");
	prefilterEnvironmentPipeline = createReloadablePipeline("prefilter_environment_cs");
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
}

ref<dx_texture> equirectangularToCubemap(dx_command_list* cl, const ref<dx_texture>& equirectangular, uint32 resolution, uint32 numMips, DXGI_FORMAT format)
{
	assert(equirectangular->resource);

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

ref<dx_texture> cubemapToIrradiance(dx_command_list* cl, const ref<dx_texture>& environment, uint32 resolution, uint32 sourceSlice)
{
	assert(environment->resource);

	CD3DX12_RESOURCE_DESC irradianceDesc(environment->resource->GetDesc());

	if (isSRGBFormat(irradianceDesc.Format))
	{
		std::cout << "Warning: Irradiance of sRGB-Format!\n";
	}

	irradianceDesc.Width = irradianceDesc.Height = resolution;
	irradianceDesc.DepthOrArraySize = 6;
	irradianceDesc.MipLevels = 1;

	if (isUAVCompatibleFormat(irradianceDesc.Format))
	{
		irradianceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	ref<dx_texture> irradiance = createTexture(irradianceDesc, 0, 0);

	irradianceDesc = CD3DX12_RESOURCE_DESC(irradiance->resource->GetDesc());

	dx_resource irradianceResource = irradiance->resource;
	SET_NAME(irradianceResource, "Irradiance");
	dx_resource stagingResource = irradianceResource;

	ref<dx_texture> stagingTexture = make_ref<dx_texture>();
	stagingTexture->resource = irradianceResource;


	if ((irradianceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		CD3DX12_RESOURCE_DESC stagingDesc = irradianceDesc;
		stagingDesc.Format = getUAVCompatibleFormat(irradianceDesc.Format);
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

		SET_NAME(stagingResource, "Staging resource for cubemap to irradiance");

		stagingTexture->resource = stagingResource;
	}

	cl->setPipelineState(*cubemapToIrradiancePipeline.pipeline);
	cl->setComputeRootSignature(*cubemapToIrradiancePipeline.rootSignature);

	cubemap_to_irradiance_cb cubemapToIrradianceCB;
	cubemapToIrradianceCB.irradianceMapSize = resolution;


	dx_descriptor_range descriptors = dxContext.frameDescriptorAllocator.allocateContiguousDescriptorRange(2);
	cl->setDescriptorHeap(descriptors);

	dx_double_descriptor_handle uavOffset = descriptors.pushHandle();
	uavOffset.create2DTextureArrayUAV(stagingTexture, 0, getUAVCompatibleFormat(irradianceDesc.Format));

	dx_double_descriptor_handle srvOffset = descriptors.pushHandle();
	if (sourceSlice == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		srvOffset.createCubemapSRV(environment);
	}
	else
	{
		srvOffset.createCubemapArraySRV(environment, { 0, 1 }, sourceSlice, 1);
	}

	cl->setCompute32BitConstants(0, cubemapToIrradianceCB);
	cl->setComputeDescriptorTable(1, srvOffset);
	cl->setComputeDescriptorTable(2, uavOffset);

	cl->dispatch(bucketize(cubemapToIrradianceCB.irradianceMapSize, 16), bucketize(cubemapToIrradianceCB.irradianceMapSize, 16), 6);

	cl->uavBarrier(stagingResource);

	if (stagingResource != irradianceResource)
	{
		cl->copyResource(stagingTexture->resource, irradiance->resource);
		cl->transitionBarrier(irradiance->resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	}

	return irradiance;
}

ref<dx_texture> prefilterEnvironment(dx_command_list* cl, const ref<dx_texture>& environment, uint32 resolution)
{
	assert(environment->resource);

	CD3DX12_RESOURCE_DESC prefilteredDesc(environment->resource->GetDesc());
	prefilteredDesc.Width = prefilteredDesc.Height = resolution;
	prefilteredDesc.DepthOrArraySize = 6;
	prefilteredDesc.MipLevels = 0;

	if (isUAVCompatibleFormat(prefilteredDesc.Format))
	{
		prefilteredDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	ref<dx_texture> prefiltered = createTexture(prefilteredDesc, 0, 0);

	prefilteredDesc = CD3DX12_RESOURCE_DESC(prefiltered->resource->GetDesc());

	dx_resource prefilteredResource = prefiltered->resource;
	SET_NAME(prefilteredResource, "Prefiltered");

	dx_resource stagingResource = prefilteredResource;

	ref<dx_texture> stagingTexture = make_ref<dx_texture>();
	stagingTexture->resource = prefilteredResource;


	if ((prefilteredDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
	{
		CD3DX12_RESOURCE_DESC stagingDesc = prefilteredDesc;
		stagingDesc.Format = getUAVCompatibleFormat(prefilteredDesc.Format);
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

		stagingTexture->resource = stagingResource;

		SET_NAME(stagingResource, "Staging resource for environment prefiltering");
	}

	cl->setPipelineState(*prefilterEnvironmentPipeline.pipeline);
	cl->setComputeRootSignature(*prefilterEnvironmentPipeline.rootSignature);

	prefilter_environment_cb prefilterEnvironmentCB;
	prefilterEnvironmentCB.totalNumMipLevels = prefilteredDesc.MipLevels;

	dx_descriptor_range descriptors = dxContext.frameDescriptorAllocator.allocateContiguousDescriptorRange(prefilteredDesc.MipLevels + 1);
	cl->setDescriptorHeap(descriptors);

	dx_double_descriptor_handle srvOffset = descriptors.pushHandle();
	srvOffset.createCubemapSRV(environment);
	cl->setComputeDescriptorTable(1, srvOffset);

	for (uint32 mipSlice = 0; mipSlice < prefilteredDesc.MipLevels; )
	{
		// Maximum number of mips to generate per pass is 5.
		uint32 numMips = min(5u, prefilteredDesc.MipLevels - mipSlice);

		prefilterEnvironmentCB.firstMip = mipSlice;
		prefilterEnvironmentCB.cubemapSize = max((uint32)prefilteredDesc.Width, prefilteredDesc.Height) >> mipSlice;
		prefilterEnvironmentCB.numMipLevelsToGenerate = numMips;

		cl->setCompute32BitConstants(0, prefilterEnvironmentCB);

		for (uint32 mip = 0; mip < numMips; ++mip)
		{
			dx_double_descriptor_handle h = descriptors.pushHandle();
			h.create2DTextureArrayUAV(stagingTexture, mipSlice + mip, getUAVCompatibleFormat(prefilteredDesc.Format));
			if (mip == 0)
			{
				cl->setComputeDescriptorTable(2, h);
			}
		}

		cl->dispatch(bucketize(prefilterEnvironmentCB.cubemapSize, 16), bucketize(prefilterEnvironmentCB.cubemapSize, 16), 6);

		mipSlice += numMips;
	}

	cl->uavBarrier(stagingResource);

	if (stagingResource != prefilteredResource)
	{
		cl->copyResource(stagingTexture->resource, prefiltered->resource);
		cl->transitionBarrier(prefiltered->resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	}

	return prefiltered;
}

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

	dx_resource brdfResource = brdf->resource;
	SET_NAME(brdfResource, "BRDF");

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

void cubemapToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment, ref<dx_buffer> shBuffer, uint32 shIndex)
{
	cl->setPipelineState(*cubemapToIrradianceSHPipeline.pipeline);
	cl->setComputeRootSignature(*cubemapToIrradianceSHPipeline.rootSignature);

	assert(environment->width == environment->height);
	assert(environment->width >= 64);
	assert(environment->depth == 6);
	assert(shBuffer->elementCount > shIndex);

	cubemap_to_irradiance_sh_cb cb;
	cb.mipLevel = environment->numMipLevels == 1 ? 0 : (environment->numMipLevels - 6);

	cl->setCompute32BitConstants(0, cb);
	cl->setDescriptorHeapSRV(1, 0, environment);
	cl->setRootComputeUAV(2, shBuffer->gpuVirtualAddress + sizeof(spherical_harmonics) * shIndex);

	cl->dispatch(1, 1);
	cl->uavBarrier(shBuffer);
}

ref<dx_buffer> cubemapToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment)
{
	ref<dx_buffer> buffer = createBuffer(sizeof(spherical_harmonics), 1, 0, true);
	cubemapToIrradianceSH(cl, environment, buffer, 0);
	return buffer;
}

