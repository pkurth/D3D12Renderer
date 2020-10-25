#include "pch.h"
#include "dx_render_primitives.h"
#include "dx_command_list.h"
#include "dx_context.h"

void* mapBuffer(dx_buffer& buffer)
{
	void* result;
	buffer.resource->Map(0, 0, &result);
	return result;
}

void unmapBuffer(dx_buffer& buffer)
{
	buffer.resource->Unmap(0, 0);
}

void uploadBufferData(dx_context* context, dx_buffer& buffer, const void* bufferData)
{
	dx_command_list* commandList = context->getFreeCopyCommandList();

	dx_resource intermediateResource;

	checkResult(context->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer.totalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = bufferData;
	subresourceData.RowPitch = buffer.totalSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	commandList->transitionBarrier(buffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	UpdateSubresources(commandList->commandList.Get(),
		buffer.resource.Get(), intermediateResource.Get(),
		0, 0, 1, &subresourceData);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.

	context->retireObject(intermediateResource);
	context->executeCommandList(commandList);
}

void updateBufferDataRange(dx_context* context, dx_buffer& buffer, const void* data, uint32 offset, uint32 size)
{
	assert(offset + size <= buffer.totalSize);

	dx_command_list* commandList = context->getFreeCopyCommandList();

	dx_resource intermediateResource;

	checkResult(context->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)));

	commandList->transitionBarrier(buffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	void* mapped;
	checkResult(intermediateResource->Map(0, 0, &mapped));
	memcpy(mapped, data, size);
	intermediateResource->Unmap(0, 0);

	commandList->commandList->CopyBufferRegion(buffer.resource.Get(), offset, intermediateResource.Get(), 0, size);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.

	context->retireObject(intermediateResource);
	context->executeCommandList(commandList);
}

static void initializeBuffer(dx_context* context, dx_buffer& buffer, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess,
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT)
{
	D3D12_RESOURCE_FLAGS flags = allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	buffer.elementSize = elementSize;
	buffer.elementCount = elementCount;
	buffer.totalSize = elementSize * elementCount;

	checkResult(context->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer.totalSize, flags),
		initialState,
		0,
		IID_PPV_ARGS(&buffer.resource)));
	buffer.gpuVirtualAddress = buffer.resource->GetGPUVirtualAddress();

	if (data)
	{
		if (heapType == D3D12_HEAP_TYPE_DEFAULT)
		{
			uploadBufferData(context, buffer, data);
		}
		else if (heapType == D3D12_HEAP_TYPE_UPLOAD)
		{
			void* dataPtr = mapBuffer(buffer);
			memcpy(dataPtr, data, buffer.totalSize);
			unmapBuffer(buffer);
		}
	}
}

dx_buffer createBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState)
{
	dx_buffer result;
	initializeBuffer(context, result, elementSize, elementCount, data, allowUnorderedAccess, initialState, D3D12_HEAP_TYPE_DEFAULT);
	return result;
}

dx_buffer createUploadBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess)
{
	dx_buffer result;
	initializeBuffer(context, result, elementSize, elementCount, data, allowUnorderedAccess, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	return result;
}

dx_vertex_buffer createVertexBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess)
{
	dx_vertex_buffer result;
	initializeBuffer(context, result, elementSize, elementCount, data, allowUnorderedAccess);
	result.view.BufferLocation = result.gpuVirtualAddress;
	result.view.SizeInBytes = result.totalSize;
	result.view.StrideInBytes = elementSize;
	return result;
}

dx_vertex_buffer createUploadVertexBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess)
{
	dx_vertex_buffer result;
	initializeBuffer(context, result, elementSize, elementCount, data, allowUnorderedAccess, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	result.view.BufferLocation = result.gpuVirtualAddress;
	result.view.SizeInBytes = result.totalSize;
	result.view.StrideInBytes = elementSize;
	return result;
}

DXGI_FORMAT getIndexBufferFormat(uint32 elementSize)
{
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if (elementSize == 1)
	{
		result = DXGI_FORMAT_R8_UINT;
	}
	else if (elementSize == 2)
	{
		result = DXGI_FORMAT_R16_UINT;
	}
	else if (elementSize == 4)
	{
		result = DXGI_FORMAT_R32_UINT;
	}
	return result;
}

dx_index_buffer createIndexBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess)
{
	dx_index_buffer result;
	initializeBuffer(context, result, elementSize, elementCount, data, allowUnorderedAccess);
	result.view.BufferLocation = result.gpuVirtualAddress;
	result.view.SizeInBytes = result.totalSize;
	result.view.Format = getIndexBufferFormat(elementSize);
	return result;
}

static void uploadTextureSubresourceData(dx_context* context, dx_texture& texture, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources)
{
	dx_command_list* commandList = context->getFreeCopyCommandList();
	commandList->transitionBarrier(texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	UINT64 requiredSize = GetRequiredIntermediateSize(texture.resource.Get(), firstSubresource, numSubresources);

	dx_resource intermediateResource;
	checkResult(context->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)
	));

	UpdateSubresources<128>(commandList->commandList.Get(), texture.resource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);
	context->retireObject(intermediateResource);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.
	//commandList->transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

	context->executeCommandList(commandList);
}

dx_texture createTexture(dx_context* context, D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState)
{
	dx_texture result;

	checkResult(context->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		0,
		IID_PPV_ARGS(&result.resource)));

	result.formatSupport.Format = textureDesc.Format;
	checkResult(context->device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));


	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		formatSupportsRTV(result))
	{
		result.rtvHandles = context->rtvAllocator.pushRenderTargetView(result);
	}

	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0 &&
		(formatSupportsDSV(result) || isDepthFormat(textureDesc.Format)))
	{
		result.dsvHandle = context->dsvAllocator.pushDepthStencilView(result);
	}

	if (subresourceData)
	{
		uploadTextureSubresourceData(context, result, subresourceData, 0, numSubresources);
	}

	return result;
}

dx_texture createTexture(dx_context* context, const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		| (allowRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		;

	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, flags);

	uint32 formatSize = getFormatSize(textureDesc.Format);

	if (data)
	{
		D3D12_SUBRESOURCE_DATA subresource;
		subresource.RowPitch = width * formatSize;
		subresource.SlicePitch = width * height * formatSize;
		subresource.pData = data;

		return createTexture(context, textureDesc, &subresource, 1, initialState);
	}
	else
	{
		return createTexture(context, textureDesc, 0, 0, initialState);
	}
}

dx_texture createDepthTexture(dx_context* context, uint32 width, uint32 height, DXGI_FORMAT format)
{
	dx_texture result;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height,
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	checkResult(context->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(&result.resource)
	));

	result.formatSupport.Format = format;
	checkResult(context->device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	result.dsvHandle = context->dsvAllocator.pushDepthStencilView(result);

	return result;
}

void resizeTexture(dx_context* context, dx_texture& texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	D3D12_RESOURCE_STATES state = initialState;
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	D3D12_CLEAR_VALUE* clearValue = 0;

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		optimizedClearValue.Format = desc.Format;
		optimizedClearValue.DepthStencil = { 1.f, 0 };
		clearValue = &optimizedClearValue;
	}

	context->retireObject(texture.resource);

	desc.Width = newWidth;
	desc.Height = newHeight;

	checkResult(context->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(&texture.resource)
	));

	if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		formatSupportsRTV(texture))
	{
		context->rtvAllocator.createRenderTargetView(texture, texture.rtvHandles);
	}

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		context->dsvAllocator.createDepthStencilView(texture, texture.dsvHandle);
	}
}

dx_root_signature createRootSignature(dx_context* context, dx_blob rootSignatureBlob)
{
	dx_root_signature rootSignature;

	checkResult(context->device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	return rootSignature;
}

dx_root_signature createRootSignature(dx_context* context, const wchar* path)
{
	dx_blob rootSignatureBlob;
	checkResult(D3DReadFileToBlob(path, &rootSignatureBlob));

	return createRootSignature(context, rootSignatureBlob);
}

dx_root_signature createRootSignature(dx_context* context, const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(context->device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(desc.NumParameters, desc.pParameters, desc.NumStaticSamplers, desc.pStaticSamplers, desc.Flags);

	dx_blob rootSignatureBlob;
	dx_blob errorBlob;
	checkResult(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	return createRootSignature(context, rootSignatureBlob);
}

dx_root_signature createRootSignature(dx_context* context, CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = numRootParameters;
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = numSamplers;
	return createRootSignature(context, rootSignatureDesc);
}

dx_root_signature createRootSignature(dx_context* context, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	dx_blob rootSignatureBlob;
	dx_blob errorBlob;
	checkResult(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob));

	dx_root_signature rootSignature;

	checkResult(context->device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	return rootSignature;
}

dx_root_signature createRootSignature(dx_context* context, CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = numRootParameters;
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = numSamplers;
	return createRootSignature(context, rootSignatureDesc);
}

dx_root_signature createRootSignature(dx_context* context, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	return createRootSignature(context, rootSignatureDesc);
}

dx_command_signature createCommandSignature(dx_context* context, dx_root_signature rootSignature, const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc)
{
	dx_command_signature commandSignature;
	checkResult(context->device->CreateCommandSignature(&commandSignatureDesc,
		commandSignatureDesc.NumArgumentDescs == 1 ? 0 : rootSignature.Get(),
		IID_PPV_ARGS(&commandSignature)));
	return commandSignature;
}

dx_command_signature createCommandSignature(dx_context* context, dx_root_signature rootSignature, D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs, uint32 numArgumentDescs, uint32 commandStructureSize)
{
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc;
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = numArgumentDescs;
	commandSignatureDesc.ByteStride = commandStructureSize;
	commandSignatureDesc.NodeMask = 0;

	return createCommandSignature(context, rootSignature, commandSignatureDesc);
}

static void initializeDescriptorHeap(dx_context* context, dx_descriptor_heap& descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;
	if (shaderVisible)
	{
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}

	checkResult(context->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap.descriptorHeap)));

	descriptorHeap.type = type;
	descriptorHeap.maxNumDescriptors = numDescriptors;
	descriptorHeap.descriptorHandleIncrementSize = context->device->GetDescriptorHandleIncrementSize(type);
	descriptorHeap.base.cpuHandle = descriptorHeap.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	descriptorHeap.base.gpuHandle = descriptorHeap.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

dx_cbv_srv_uav_descriptor_heap createDescriptorHeap(dx_context* context, uint32 numDescriptors, bool shaderVisible)
{
	dx_cbv_srv_uav_descriptor_heap descriptorHeap;
	initializeDescriptorHeap(context, descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, numDescriptors, shaderVisible);
	descriptorHeap.pushIndex = 0;
	return descriptorHeap;
}

dx_rtv_descriptor_heap createRTVDescriptorAllocator(dx_context* context, uint32 numDescriptors)
{
	dx_rtv_descriptor_heap descriptorHeap;
	initializeDescriptorHeap(context, descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numDescriptors, false);
	descriptorHeap.pushIndex = 0;
	return descriptorHeap;
}

dx_dsv_descriptor_heap createDSVDescriptorAllocator(dx_context* context, uint32 numDescriptors)
{
	dx_dsv_descriptor_heap descriptorHeap;
	initializeDescriptorHeap(context, descriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, numDescriptors, false);
	descriptorHeap.pushIndex = 0;
	return descriptorHeap;
}

static dx_descriptor_handle create2DTextureSRV(dx_device device, dx_texture& texture, dx_descriptor_handle index, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture.resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = mipRange.first;
	srvDesc.Texture2D.MipLevels = mipRange.count;

	device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createCubemapSRV(dx_device device, dx_texture& texture, dx_descriptor_handle index, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture.resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = mipRange.first;
	srvDesc.TextureCube.MipLevels = mipRange.count;

	device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createCubemapArraySRV(dx_device device, dx_texture& texture, dx_descriptor_handle index, texture_mip_range mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = (overrideFormat == DXGI_FORMAT_UNKNOWN) ? texture.resource->GetDesc().Format : overrideFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc.TextureCubeArray.MostDetailedMip = mipRange.first;
	srvDesc.TextureCubeArray.MipLevels = mipRange.count;
	srvDesc.TextureCubeArray.NumCubes = numCubes;
	srvDesc.TextureCubeArray.First2DArrayFace = firstCube * 6;

	device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createDepthTextureSRV(dx_device device, dx_texture& texture, dx_descriptor_handle index)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = getReadFormatFromTypeless(texture.resource->GetDesc().Format);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createNullTextureSRV(dx_device device, dx_descriptor_handle index)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 0;

	device->CreateShaderResourceView(0, &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createBufferSRV(dx_device device, dx_buffer& buffer, dx_descriptor_handle index, buffer_range bufferRange = {})
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = bufferRange.firstElement;
	srvDesc.Buffer.NumElements = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	srvDesc.Buffer.StructureByteStride = buffer.elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	device->CreateShaderResourceView(buffer.resource.Get(), &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createRawBufferSRV(dx_device device, dx_buffer& buffer, dx_descriptor_handle index, buffer_range bufferRange = {})
{
	uint32 firstElementByteOffset = bufferRange.firstElement * buffer.elementSize;
	assert(firstElementByteOffset % 16 == 0);

	uint32 count = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	uint32 totalSize = count * buffer.elementSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = firstElementByteOffset / 4;
	srvDesc.Buffer.NumElements = totalSize / 4;
	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	device->CreateShaderResourceView(buffer.resource.Get(), &srvDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle create2DTextureUAV(dx_device device, dx_texture& texture, dx_descriptor_handle index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2D.MipSlice = mipSlice;
	device->CreateUnorderedAccessView(texture.resource.Get(), 0, &uavDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle create2DTextureArrayUAV(dx_device device, dx_texture& texture, dx_descriptor_handle index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = overrideFormat;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.ArraySize = texture.resource->GetDesc().DepthOrArraySize;
	uavDesc.Texture2DArray.MipSlice = mipSlice;
	device->CreateUnorderedAccessView(texture.resource.Get(), 0, &uavDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createNullTextureUAV(dx_device device, dx_descriptor_handle index)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	device->CreateUnorderedAccessView(0, 0, &uavDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createBufferUAV(dx_device device, dx_buffer& buffer, dx_descriptor_handle index, buffer_range bufferRange = {})
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.FirstElement = bufferRange.firstElement;
	uavDesc.Buffer.NumElements = (bufferRange.numElements != -1) ? bufferRange.numElements : (buffer.elementCount - bufferRange.firstElement);
	uavDesc.Buffer.StructureByteStride = buffer.elementSize;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(buffer.resource.Get(), 0, &uavDesc, index.cpuHandle);

	return index;
}

static dx_descriptor_handle createRaytracingAccelerationStructureSRV(dx_device device, dx_buffer& tlas, dx_descriptor_handle index)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlas.gpuVirtualAddress;

	device->CreateShaderResourceView(nullptr, &srvDesc, index.cpuHandle);

	return index;
}

dx_descriptor_handle dx_descriptor_range::create2DTextureSRV(dx_texture& texture, dx_descriptor_handle handle, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	return ::create2DTextureSRV(getDevice(descriptorHeap), texture, handle, mipRange, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::create2DTextureSRV(dx_texture& texture, uint32 index, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	return ::create2DTextureSRV(getDevice(descriptorHeap), texture, getHandle(*this, index), mipRange, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::push2DTextureSRV(dx_texture& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	return create2DTextureSRV(texture, pushIndex++, mipRange, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::createCubemapSRV(dx_texture& texture, dx_descriptor_handle handle, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	return ::createCubemapSRV(getDevice(descriptorHeap), texture, handle, mipRange, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::createCubemapSRV(dx_texture& texture, uint32 index, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	return ::createCubemapSRV(getDevice(descriptorHeap), texture, getHandle(*this, index), mipRange, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::pushCubemapSRV(dx_texture& texture, texture_mip_range mipRange, DXGI_FORMAT overrideFormat)
{
	return createCubemapSRV(texture, pushIndex++, mipRange, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::createCubemapArraySRV(dx_texture& texture, dx_descriptor_handle handle, texture_mip_range mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat)
{
	return ::createCubemapArraySRV(getDevice(descriptorHeap), texture, handle, mipRange, firstCube, numCubes, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::createCubemapArraySRV(dx_texture& texture, uint32 index, texture_mip_range mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat)
{
	return ::createCubemapArraySRV(getDevice(descriptorHeap), texture, getHandle(*this, index), mipRange, firstCube, numCubes, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::pushCubemapArraySRV(dx_texture& texture, texture_mip_range mipRange, uint32 firstCube, uint32 numCubes, DXGI_FORMAT overrideFormat)
{
	return createCubemapArraySRV(texture, pushIndex++, mipRange, firstCube, numCubes, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::createDepthTextureSRV(dx_texture& texture, dx_descriptor_handle handle)
{
	return ::createDepthTextureSRV(getDevice(descriptorHeap), texture, handle);
}

dx_descriptor_handle dx_descriptor_range::createDepthTextureSRV(dx_texture& texture, uint32 index)
{
	return ::createDepthTextureSRV(getDevice(descriptorHeap), texture, getHandle(*this, index));
}

dx_descriptor_handle dx_descriptor_range::pushDepthTextureSRV(dx_texture& texture)
{
	return createDepthTextureSRV(texture, pushIndex++);
}

dx_descriptor_handle dx_descriptor_range::createNullTextureSRV(dx_descriptor_handle handle)
{
	return ::createNullTextureSRV(getDevice(descriptorHeap), handle);
}

dx_descriptor_handle dx_descriptor_range::createNullTextureSRV(uint32 index)
{
	return ::createNullTextureSRV(getDevice(descriptorHeap), getHandle(*this, index));
}

dx_descriptor_handle dx_descriptor_range::pushNullTextureSRV()
{
	return createNullTextureSRV(pushIndex++);
}

dx_descriptor_handle dx_descriptor_range::createBufferSRV(dx_buffer& buffer, dx_descriptor_handle handle, buffer_range bufferRange)
{
	return ::createBufferSRV(getDevice(descriptorHeap), buffer, handle, bufferRange);
}

dx_descriptor_handle dx_descriptor_range::createBufferSRV(dx_buffer& buffer, uint32 index, buffer_range bufferRange)
{
	return ::createBufferSRV(getDevice(descriptorHeap), buffer, getHandle(*this, index), bufferRange);
}

dx_descriptor_handle dx_descriptor_range::pushBufferSRV(dx_buffer& buffer, buffer_range bufferRange)
{
	return createBufferSRV(buffer, pushIndex++, bufferRange);
}

dx_descriptor_handle dx_descriptor_range::createRawBufferSRV(dx_buffer& buffer, dx_descriptor_handle handle, buffer_range bufferRange)
{
	return ::createRawBufferSRV(getDevice(descriptorHeap), buffer, handle, bufferRange);
}

dx_descriptor_handle dx_descriptor_range::createRawBufferSRV(dx_buffer& buffer, uint32 index, buffer_range bufferRange)
{
	return ::createRawBufferSRV(getDevice(descriptorHeap), buffer, getHandle(*this, index), bufferRange);
}

dx_descriptor_handle dx_descriptor_range::pushRawBufferSRV(dx_buffer& buffer, buffer_range bufferRange)
{
	return createRawBufferSRV(buffer, pushIndex++, bufferRange);
}

dx_descriptor_handle dx_descriptor_range::create2DTextureUAV(dx_texture& texture, dx_descriptor_handle handle, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	return ::create2DTextureUAV(getDevice(descriptorHeap), texture, handle, mipSlice, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::create2DTextureUAV(dx_texture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	return ::create2DTextureUAV(getDevice(descriptorHeap), texture, getHandle(*this, index), mipSlice, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::push2DTextureUAV(dx_texture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	return create2DTextureUAV(texture, pushIndex++, mipSlice, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::create2DTextureArrayUAV(dx_texture& texture, dx_descriptor_handle handle, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	return ::create2DTextureArrayUAV(getDevice(descriptorHeap), texture, handle, mipSlice, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::create2DTextureArrayUAV(dx_texture& texture, uint32 index, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	return ::create2DTextureArrayUAV(getDevice(descriptorHeap), texture, getHandle(*this, index), mipSlice, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::push2DTextureArrayUAV(dx_texture& texture, uint32 mipSlice, DXGI_FORMAT overrideFormat)
{
	return create2DTextureArrayUAV(texture, pushIndex++, mipSlice, overrideFormat);
}

dx_descriptor_handle dx_descriptor_range::createNullTextureUAV(dx_descriptor_handle handle)
{
	return ::createNullTextureUAV(getDevice(descriptorHeap), handle);
}

dx_descriptor_handle dx_descriptor_range::createNullTextureUAV(uint32 index)
{
	return ::createNullTextureUAV(getDevice(descriptorHeap), getHandle(*this, index));
}

dx_descriptor_handle dx_descriptor_range::pushNullTextureUAV()
{
	return createNullTextureUAV(pushIndex++);
}

dx_descriptor_handle dx_descriptor_range::createBufferUAV(dx_buffer& buffer, dx_descriptor_handle handle, buffer_range bufferRange)
{
	return ::createBufferUAV(getDevice(descriptorHeap), buffer, handle, bufferRange);
}

dx_descriptor_handle dx_descriptor_range::createBufferUAV(dx_buffer& buffer, uint32 index, buffer_range bufferRange)
{
	return ::createBufferUAV(getDevice(descriptorHeap), buffer, getHandle(*this, index), bufferRange);
}

dx_descriptor_handle dx_descriptor_range::pushBufferUAV(dx_buffer& buffer, buffer_range bufferRange)
{
	return createBufferUAV(buffer, pushIndex++, bufferRange);
}

dx_descriptor_handle dx_descriptor_range::createRaytracingAccelerationStructureSRV(dx_buffer& tlas, dx_descriptor_handle handle)
{
	return ::createRaytracingAccelerationStructureSRV(getDevice(descriptorHeap), tlas, handle);
}

dx_descriptor_handle dx_descriptor_range::createRaytracingAccelerationStructureSRV(dx_buffer& tlas, uint32 index)
{
	return ::createRaytracingAccelerationStructureSRV(getDevice(descriptorHeap), tlas, getHandle(*this, index));
}

dx_descriptor_handle dx_descriptor_range::pushRaytracingAccelerationStructureSRV(dx_buffer& tlas)
{
	return createRaytracingAccelerationStructureSRV(tlas, pushIndex++);
}

dx_descriptor_handle dx_rtv_descriptor_heap::pushRenderTargetView(dx_texture& texture)
{
	D3D12_RESOURCE_DESC resourceDesc = texture.resource->GetDesc();
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	uint32 slices = resourceDesc.DepthOrArraySize;

	uint32 index = atomicAdd(pushIndex, slices);
	dx_descriptor_handle result = getHandle(*this, index);
	return createRenderTargetView(texture, result);
}

dx_descriptor_handle dx_rtv_descriptor_heap::createRenderTargetView(dx_texture& texture, dx_descriptor_handle index)
{
	D3D12_RESOURCE_DESC resourceDesc = texture.resource->GetDesc();
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	uint32 slices = resourceDesc.DepthOrArraySize;

	if (slices > 1)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = resourceDesc.Format;
		rtvDesc.Texture2DArray.ArraySize = 1;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		for (uint32 i = 0; i < slices; ++i)
		{
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(index.cpuHandle, i, descriptorHandleIncrementSize);
			getDevice(descriptorHeap)->CreateRenderTargetView(texture.resource.Get(), &rtvDesc, rtv);
		}
	}
	else
	{
		getDevice(descriptorHeap)->CreateRenderTargetView(texture.resource.Get(), 0, index.cpuHandle);
	}

	return index;
}

dx_descriptor_handle dx_dsv_descriptor_heap::pushDepthStencilView(dx_texture& texture)
{
	D3D12_RESOURCE_DESC resourceDesc = texture.resource->GetDesc();
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	uint32 slices = resourceDesc.DepthOrArraySize;

	uint32 index = atomicAdd(pushIndex, slices);
	dx_descriptor_handle result = getHandle(*this, index);
	return createDepthStencilView(texture, result);
}

dx_descriptor_handle dx_dsv_descriptor_heap::createDepthStencilView(dx_texture& texture, dx_descriptor_handle index)
{
	D3D12_RESOURCE_DESC resourceDesc = texture.resource->GetDesc();
	DXGI_FORMAT format = resourceDesc.Format;
	if (isDepthFormat(format))
	{
		resourceDesc.Format = getTypelessFormat(format);
	}

	assert(resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
	assert(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	if (isDepthFormat(format))
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = format;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		getDevice(descriptorHeap)->CreateDepthStencilView(texture.resource.Get(), &dsvDesc, index.cpuHandle);
	}
	else
	{
		getDevice(descriptorHeap)->CreateDepthStencilView(texture.resource.Get(), 0, index.cpuHandle);
	}

	return index;
}

dx_frame_descriptor_allocator createFrameDescriptorAllocator(dx_context* context)
{
	dx_frame_descriptor_allocator result = {};
	result.device = context->device;
	result.mutex = createMutex();
	result.currentFrame = NUM_BUFFERED_FRAMES - 1;
	return result;
}

void dx_frame_descriptor_allocator::newFrame(uint32 bufferedFrameID)
{
	lock(mutex);

	currentFrame = bufferedFrameID;

	while (usedPages[currentFrame])
	{
		dx_descriptor_page* page = usedPages[currentFrame];
		usedPages[currentFrame] = page->next;
		page->next = freePages;
		freePages = page;
	}

	unlock(mutex);
}

dx_descriptor_range dx_frame_descriptor_allocator::allocateContiguousDescriptorRange(uint32 count)
{
	lock(mutex);

	dx_descriptor_page* current = usedPages[currentFrame];
	if (!current || (current->maxNumDescriptors - current->usedDescriptors < count))
	{
		dx_descriptor_page* freePage = freePages;
		if (!freePage)
		{
			freePage = (dx_descriptor_page*)calloc(1, sizeof(dx_descriptor_page));

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = 1024;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			checkResult(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&freePage->descriptorHeap)));

			freePage->maxNumDescriptors = desc.NumDescriptors;
			freePage->descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(desc.Type);
			freePage->base.cpuHandle = freePage->descriptorHeap->GetCPUDescriptorHandleForHeapStart();
			freePage->base.gpuHandle = freePage->descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		freePage->usedDescriptors = 0;
		freePage->next = current;
		usedPages[currentFrame] = freePage;
		current = freePage;
	}

	uint32 index = current->usedDescriptors;
	current->usedDescriptors += count;

	unlock(mutex);

	dx_descriptor_range result;

	result.base.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(current->base.gpuHandle, index, current->descriptorHandleIncrementSize);
	result.base.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(current->base.cpuHandle, index, current->descriptorHandleIncrementSize);
	result.descriptorHandleIncrementSize = current->descriptorHandleIncrementSize;
	result.descriptorHeap = current->descriptorHeap;
	result.maxNumDescriptors = count;
	result.pushIndex = 0;

	return result;
}

uint32 dx_render_target::pushColorAttachment(dx_texture& texture)
{
	assert(texture.resource);

	uint32 attachmentPoint = numAttachments++;

	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	rtvHandles[attachmentPoint] = texture.rtvHandles.cpuHandle;

	if (width == 0 || height == 0)
	{
		width = (uint32)desc.Width;
		height = desc.Height;
		viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
	}
	else
	{
		assert(width == desc.Width && height == desc.Height);
	}

	++renderTargetFormat.NumRenderTargets;
	renderTargetFormat.RTFormats[attachmentPoint] = desc.Format;

	return attachmentPoint;
}

void dx_render_target::pushDepthStencilAttachment(dx_texture& texture)
{
	assert(texture.resource);

	dsvHandle = texture.dsvHandle.cpuHandle;

	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	if (width == 0 || height == 0)
	{
		width = (uint32)desc.Width;
		height = desc.Height;
		viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
	}
	else
	{
		assert(width == desc.Width && height == desc.Height);
	}

	depthStencilFormat = getDepthFormatFromTypeless(desc.Format);
}

void dx_render_target::notifyOnTextureResize(uint32 width, uint32 height)
{
	this->width = width;
	this->height = height;
	viewport = { 0, 0, (float)width, (float)height, 0.f, 1.f };
}

dx_submesh createSubmesh(dx_mesh& mesh, submesh_info info)
{
	dx_submesh result;
	result.vertexBuffer = mesh.vertexBuffer;
	result.indexBuffer = mesh.indexBuffer;
	result.baseVertex = info.baseVertex;
	result.firstTriangle = info.firstTriangle;
	result.numTriangles = info.numTriangles;
	return result;
}

dx_submesh createSubmesh(dx_mesh& mesh)
{
	dx_submesh result;
	result.vertexBuffer = mesh.vertexBuffer;
	result.indexBuffer = mesh.indexBuffer;
	result.baseVertex = 0;
	result.firstTriangle = 0;
	result.numTriangles = mesh.indexBuffer.elementCount / 3;
	return result;
}

barrier_batcher::barrier_batcher(dx_command_list* cl)
{
	this->cl = cl;
}

barrier_batcher& barrier_batcher::transition(dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource);
	return *this;
}

barrier_batcher& barrier_batcher::uav(dx_resource resource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	return *this;
}

barrier_batcher& barrier_batcher::aliasing(dx_resource before, dx_resource after)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Aliasing(before.Get(), after.Get());
	return *this;
}

void barrier_batcher::submit()
{
	if (numBarriers)
	{
		cl->barriers(barriers, numBarriers);
		numBarriers = 0;
	}
}
