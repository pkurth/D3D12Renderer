#include "pch.h"
#include "dx_render_primitives.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "dx_descriptor_allocation.h"

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

void uploadBufferData(dx_buffer& buffer, const void* bufferData)
{
	dx_command_list* commandList = dxContext.getFreeCopyCommandList();

	dx_resource intermediateResource;

	checkResult(dxContext.device->CreateCommittedResource(
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

	dxContext.retireObject(intermediateResource);
	dxContext.executeCommandList(commandList);
}

void updateBufferDataRange(dx_buffer& buffer, const void* data, uint32 offset, uint32 size)
{
	assert(offset + size <= buffer.totalSize);

	dx_command_list* commandList = dxContext.getFreeCopyCommandList();

	dx_resource intermediateResource;

	checkResult(dxContext.device->CreateCommittedResource(
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

	dxContext.retireObject(intermediateResource);
	dxContext.executeCommandList(commandList);
}

static void initializeBuffer(dx_buffer& buffer, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing,
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT)
{
	D3D12_RESOURCE_FLAGS flags = allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

	buffer.elementSize = elementSize;
	buffer.elementCount = elementCount;
	buffer.totalSize = elementSize * elementCount;
	buffer.heapType = heapType;

	checkResult(dxContext.device->CreateCommittedResource(
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
			uploadBufferData(buffer, data);
		}
		else if (heapType == D3D12_HEAP_TYPE_UPLOAD)
		{
			void* dataPtr = mapBuffer(buffer);
			memcpy(dataPtr, data, buffer.totalSize);
			unmapBuffer(buffer);
		}
	}

	buffer.defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferSRV(buffer);
	if (allowUnorderedAccess)
	{
		buffer.defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferUAV(buffer);
	}
	if (allowClearing)
	{
		buffer.cpuClearUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createBufferUintUAV(buffer);
		dx_cpu_descriptor_handle shaderVisibleCPUHandle = dxContext.descriptorAllocatorGPU.getFreeHandle().createBufferUintUAV(buffer);
		buffer.gpuClearUAV = dxContext.descriptorAllocatorGPU.getMatchingGPUHandle(shaderVisibleCPUHandle);
	}
}

dx_buffer createBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing, D3D12_RESOURCE_STATES initialState)
{
	dx_buffer result = {};
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing, initialState, D3D12_HEAP_TYPE_DEFAULT);
	return result;
}

dx_buffer createUploadBuffer(uint32 elementSize, uint32 elementCount, void* data)
{
	dx_buffer result = {};
	initializeBuffer(result, elementSize, elementCount, data, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	return result;
}

dx_vertex_buffer createVertexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing)
{
	dx_vertex_buffer result = {};
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result.view.BufferLocation = result.gpuVirtualAddress;
	result.view.SizeInBytes = result.totalSize;
	result.view.StrideInBytes = elementSize;
	return result;
}

dx_vertex_buffer createUploadVertexBuffer(uint32 elementSize, uint32 elementCount, void* data)
{
	dx_vertex_buffer result = {};
	initializeBuffer(result, elementSize, elementCount, data, false, false, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
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

dx_index_buffer createIndexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess, bool allowClearing)
{
	dx_index_buffer result = {};
	initializeBuffer(result, elementSize, elementCount, data, allowUnorderedAccess, allowClearing);
	result.view.BufferLocation = result.gpuVirtualAddress;
	result.view.SizeInBytes = result.totalSize;
	result.view.Format = getIndexBufferFormat(elementSize);
	return result;
}

void resizeBuffer(dx_buffer& buffer, uint32 newElementCount, D3D12_RESOURCE_STATES initialState)
{
	dxContext.retireObject(buffer.resource);

	buffer.elementCount = newElementCount;
	buffer.totalSize = buffer.elementCount * buffer.elementSize;

	auto desc = buffer.resource->GetDesc();

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(buffer.heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer.totalSize, desc.Flags),
		initialState,
		0,
		IID_PPV_ARGS(&buffer.resource)));
	buffer.gpuVirtualAddress = buffer.resource->GetGPUVirtualAddress();

	buffer.defaultSRV.createBufferSRV(buffer);
	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		buffer.defaultUAV.createBufferUAV(buffer);
	}
	if (buffer.cpuClearUAV.cpuHandle.ptr)
	{
		buffer.cpuClearUAV.createBufferUintUAV(buffer);
		dx_cpu_descriptor_handle shaderVisibleCPUHandle = dxContext.descriptorAllocatorGPU.getMatchingCPUHandle(buffer.gpuClearUAV);
		shaderVisibleCPUHandle.createBufferUintUAV(buffer);
	}
}

void freeBuffer(dx_buffer& buffer)
{
	if (buffer.defaultSRV.cpuHandle.ptr)
	{
		dxContext.descriptorAllocatorCPU.freeHandle(buffer.defaultSRV);
	}
	if (buffer.defaultUAV.cpuHandle.ptr)
	{
		dxContext.descriptorAllocatorCPU.freeHandle(buffer.defaultUAV);
	}
	if (buffer.cpuClearUAV.cpuHandle.ptr)
	{
		dxContext.descriptorAllocatorCPU.freeHandle(buffer.cpuClearUAV);
	}
	if (buffer.gpuClearUAV.gpuHandle.ptr)
	{
		dxContext.descriptorAllocatorGPU.freeHandle(dxContext.descriptorAllocatorGPU.getMatchingCPUHandle(buffer.gpuClearUAV));
	}
	dxContext.retireObject(buffer.resource);
	buffer = {};
}

void uploadTextureSubresourceData(dx_texture& texture, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources)
{
	dx_command_list* commandList = dxContext.getFreeCopyCommandList();
	commandList->transitionBarrier(texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);

	UINT64 requiredSize = GetRequiredIntermediateSize(texture.resource.Get(), firstSubresource, numSubresources);

	dx_resource intermediateResource;
	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&intermediateResource)
	));

	UpdateSubresources<128>(commandList->commandList.Get(), texture.resource.Get(), intermediateResource.Get(), 0, firstSubresource, numSubresources, subresourceData);
	dxContext.retireObject(intermediateResource);

	// We are omitting the transition to common here, since the resource automatically decays to common state after being accessed on a copy queue.
	//commandList->transitionBarrier(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(commandList);
}

dx_texture createTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState)
{
	dx_texture result = {};

	checkResult(dxContext.device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		initialState,
		0,
		IID_PPV_ARGS(&result.resource)));

	result.formatSupport.Format = textureDesc.Format;
	checkResult(dxContext.device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));


	if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0 &&
		formatSupportsRTV(result))
	{
		result.rtvHandles = dxContext.rtvAllocator.pushRenderTargetView(result);
	}

	if (subresourceData)
	{
		uploadTextureSubresourceData(result, subresourceData, 0, numSubresources);
	}

	if (textureDesc.DepthOrArraySize == 6)
	{
		result.defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createCubemapSRV(result);
	}
	else
	{
		result.defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureSRV(result);
	}

	if (textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		if (textureDesc.DepthOrArraySize == 6)
		{
			result.defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().createCubemapUAV(result);
		}
		else
		{
			result.defaultUAV = dxContext.descriptorAllocatorCPU.getFreeHandle().create2DTextureUAV(result);
		}
	}
	
	return result;
}

dx_texture createTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allowRenderTarget, bool allowUnorderedAccess, D3D12_RESOURCE_STATES initialState)
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

		return createTexture(textureDesc, &subresource, 1, initialState);
	}
	else
	{
		return createTexture(textureDesc, 0, 0, initialState);
	}
}

dx_texture createDepthTexture(uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength, D3D12_RESOURCE_STATES initialState)
{
	dx_texture result = {};

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.DepthStencil = { 1.f, 0 };

	DXGI_FORMAT typelessFormat = getTypelessFormat(format);

	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(typelessFormat, width, height,
		arrayLength, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	checkResult(dxContext.device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		&optimizedClearValue,
		IID_PPV_ARGS(&result.resource)
	));

	result.formatSupport.Format = format;
	checkResult(dxContext.device->CheckFeatureSupport(
		D3D12_FEATURE_FORMAT_SUPPORT,
		&result.formatSupport,
		sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

	result.dsvHandle = dxContext.dsvAllocator.pushDepthStencilView(result);
	if (arrayLength == 1)
	{
		result.defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createDepthTextureSRV(result);
	}
	else
	{
		result.defaultSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createDepthTextureArraySRV(result);
	}

	return result;
}

void resizeTexture(dx_texture& texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_DESC desc = texture.resource->GetDesc();

	D3D12_RESOURCE_STATES state = initialState;
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	D3D12_CLEAR_VALUE* clearValue = 0;

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		optimizedClearValue.Format = texture.formatSupport.Format;
		optimizedClearValue.DepthStencil = { 1.f, 0 };
		clearValue = &optimizedClearValue;
	}

	dxContext.retireObject(texture.resource);

	desc.Width = newWidth;
	desc.Height = newHeight;

	checkResult(dxContext.device->CreateCommittedResource(
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
		dxContext.rtvAllocator.createRenderTargetView(texture, texture.rtvHandles);
	}

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		dxContext.dsvAllocator.createDepthStencilView(texture, texture.dsvHandle);
		if (desc.DepthOrArraySize == 1)
		{
			texture.defaultSRV.createDepthTextureSRV(texture);
		}
		else
		{
			texture.defaultSRV.createDepthTextureArraySRV(texture);
		}
	}
	else
	{
		if (desc.DepthOrArraySize == 6)
		{
			texture.defaultSRV.createCubemapSRV(texture);
		}
		else
		{
			texture.defaultSRV.create2DTextureSRV(texture);
		}
	}

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		if (desc.DepthOrArraySize == 6)
		{
			texture.defaultUAV.createCubemapUAV(texture);
		}
		else
		{
			texture.defaultUAV.create2DTextureUAV(texture);
		}
	}
}

void freeTexture(dx_texture& texture)
{
	if (texture.defaultSRV.cpuHandle.ptr)
	{
		dxContext.descriptorAllocatorCPU.freeHandle(texture.defaultSRV);
	}
	if (texture.defaultUAV.cpuHandle.ptr)
	{
		dxContext.descriptorAllocatorCPU.freeHandle(texture.defaultUAV);
	}
	// TODO: Free RTV & DVS.

	dxContext.retireObject(texture.resource);
	texture = {};
}

static void copyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC* desc, dx_root_signature& result)
{
	uint32 numDescriptorTables = 0;
	for (uint32 i = 0; i < desc->NumParameters; ++i)
	{
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			++numDescriptorTables;
			setBit(result.tableRootParameterMask, i);
		}
	}

	result.descriptorTableSizes = new uint32[numDescriptorTables];
	result.numDescriptorTables = numDescriptorTables;

	uint32 index = 0;
	for (uint32 i = 0; i < desc->NumParameters; ++i)
	{
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			uint32 numRanges = desc->pParameters[i].DescriptorTable.NumDescriptorRanges;
			result.descriptorTableSizes[index] = 0;
			for (uint32 r = 0; r < numRanges; ++r)
			{
				result.descriptorTableSizes[index] += desc->pParameters[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
			}
			++index;
		}
	}
}

static void copyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1* desc, dx_root_signature& result)
{
	uint32 numDescriptorTables = 0;
	for (uint32 i = 0; i < desc->NumParameters; ++i)
	{
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			++numDescriptorTables;
			setBit(result.tableRootParameterMask, i);
		}
	}

	result.descriptorTableSizes = new uint32[numDescriptorTables];
	result.numDescriptorTables = numDescriptorTables;

	uint32 index = 0;
	for (uint32 i = 0; i < desc->NumParameters; ++i)
	{
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			uint32 numRanges = desc->pParameters[i].DescriptorTable.NumDescriptorRanges;
			result.descriptorTableSizes[index] = 0;
			for (uint32 r = 0; r < numRanges; ++r)
			{
				result.descriptorTableSizes[index] += desc->pParameters[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
			}
			++index;
		}
	}
}

dx_root_signature createRootSignature(dx_blob rootSignatureBlob)
{
	dx_root_signature result = {};

	checkResult(dxContext.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&result.rootSignature)));

	com<ID3D12RootSignatureDeserializer> deserializer;
	checkResult(D3D12CreateRootSignatureDeserializer(rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&deserializer)));
	D3D12_ROOT_SIGNATURE_DESC* desc = (D3D12_ROOT_SIGNATURE_DESC*)deserializer->GetRootSignatureDesc();

	copyRootSignatureDesc(desc, result);

	return result;
}

dx_root_signature createRootSignature(const wchar* path)
{
	dx_blob rootSignatureBlob;
	checkResult(D3DReadFileToBlob(path, &rootSignatureBlob));
	return createRootSignature(rootSignatureBlob);
}

dx_root_signature createRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& desc)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(dxContext.device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(desc.NumParameters, desc.pParameters, desc.NumStaticSamplers, desc.pStaticSamplers, desc.Flags);

	dx_blob rootSignatureBlob;
	dx_blob errorBlob;
	checkResult(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	dx_root_signature rootSignature = {};

	checkResult(dxContext.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature.rootSignature)));

	copyRootSignatureDesc(&desc, rootSignature);

	return rootSignature;
}

dx_root_signature createRootSignature(CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = numRootParameters;
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = numSamplers;
	return createRootSignature(rootSignatureDesc);
}

dx_root_signature createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	dx_blob rootSignatureBlob;
	dx_blob errorBlob;
	checkResult(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob));

	dx_root_signature rootSignature = {};

	checkResult(dxContext.device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature.rootSignature)));

	copyRootSignatureDesc(&desc, rootSignature);

	return rootSignature;
}

dx_root_signature createRootSignature(CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = numRootParameters;
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = numSamplers;
	return createRootSignature(rootSignatureDesc);
}

dx_root_signature createRootSignature(D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	return createRootSignature(rootSignatureDesc);
}

void freeRootSignature(dx_root_signature& rs)
{
	if (rs.descriptorTableSizes)
	{
		delete[] rs.descriptorTableSizes;
	}
}

dx_command_signature createCommandSignature(dx_root_signature rootSignature, const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc)
{
	dx_command_signature commandSignature;
	checkResult(dxContext.device->CreateCommandSignature(&commandSignatureDesc,
		commandSignatureDesc.NumArgumentDescs == 1 ? 0 : rootSignature.rootSignature.Get(),
		IID_PPV_ARGS(&commandSignature)));
	return commandSignature;
}

dx_command_signature createCommandSignature(dx_root_signature rootSignature, D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs, uint32 numArgumentDescs, uint32 commandStructureSize)
{
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc;
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = numArgumentDescs;
	commandSignatureDesc.ByteStride = commandStructureSize;
	commandSignatureDesc.NodeMask = 0;

	return createCommandSignature(rootSignature, commandSignatureDesc);
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

void dx_render_target::pushDepthStencilAttachment(dx_texture& texture, uint32 arraySlice)
{
	assert(texture.resource);

	dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(texture.dsvHandle.cpuHandle, arraySlice, dxContext.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

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

barrier_batcher::barrier_batcher(dx_command_list* cl)
{
	this->cl = cl;
}

barrier_batcher& barrier_batcher::transition(const dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(res.Get(), from, to, subresource);
	return *this;
}

barrier_batcher& barrier_batcher::transition(const dx_texture& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	return transition(res.resource, from, to, subresource);
}

barrier_batcher& barrier_batcher::transition(const dx_texture* res, uint32 count, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource)
{
	for (uint32 i = 0; i < count; ++i)
	{
		transition(res[i].resource, from, to, subresource);
	}
	return *this;
}

barrier_batcher& barrier_batcher::uav(const dx_resource& resource)
{
	if (numBarriers == arraysize(barriers))
	{
		submit();
	}

	barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());
	return *this;
}

barrier_batcher& barrier_batcher::aliasing(const dx_resource& before, const dx_resource& after)
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
