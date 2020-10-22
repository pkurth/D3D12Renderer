#pragma once

#include "dx.h"
#include "threading.h"

struct dx_texture;
struct dx_buffer;
struct dx_context;

#define UNBOUNDED_DESCRIPTOR_RANGE -1

struct texture_mip_range
{
	uint32 first = 0;
	uint32 count = (uint32)-1; // Use all mips.
};

struct buffer_range
{
	uint32 firstElement = 0;
	uint32 numElements = (uint32)-1;
};

struct dx_descriptor_handle
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};

struct dx_descriptor_heap
{
	com<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE type;

	dx_descriptor_handle base;

	uint32 maxNumDescriptors;
	uint32 descriptorHandleIncrementSize;
};

struct dx_descriptor_range : dx_descriptor_heap
{
	uint32 pushIndex;

	dx_descriptor_handle create2DTextureSRV(dx_texture& texture, uint32 index, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_descriptor_handle push2DTextureSRV(dx_texture& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	dx_descriptor_handle createCubemapSRV(dx_texture& texture, uint32 index, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_descriptor_handle pushCubemapSRV(dx_texture& texture, texture_mip_range mipRange = {}, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	dx_descriptor_handle createCubemapArraySRV(dx_texture& texture, uint32 index, texture_mip_range mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_descriptor_handle pushCubemapArraySRV(dx_texture& texture, texture_mip_range mipRange = {}, uint32 firstCube = 0, uint32 numCubes = 1, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	dx_descriptor_handle createDepthTextureSRV(dx_texture& texture, uint32 index);
	dx_descriptor_handle pushDepthTextureSRV(dx_texture& texture);

	dx_descriptor_handle createNullTextureSRV(uint32 index);
	dx_descriptor_handle pushNullTextureSRV();

	dx_descriptor_handle createBufferSRV(dx_buffer& buffer, uint32 index, buffer_range bufferRange = {});
	dx_descriptor_handle pushBufferSRV(dx_buffer& buffer, buffer_range bufferRange = {});

	dx_descriptor_handle createRawBufferSRV(dx_buffer& buffer, uint32 index, buffer_range bufferRange = {});
	dx_descriptor_handle pushRawBufferSRV(dx_buffer& buffer, buffer_range bufferRange = {});

	dx_descriptor_handle create2DTextureUAV(dx_texture& texture, uint32 index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_descriptor_handle push2DTextureUAV(dx_texture& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	dx_descriptor_handle create2DTextureArrayUAV(dx_texture& texture, uint32 index, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);
	dx_descriptor_handle push2DTextureArrayUAV(dx_texture& texture, uint32 mipSlice = 0, DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

	dx_descriptor_handle createNullTextureUAV(uint32 index);
	dx_descriptor_handle pushNullTextureUAV();

	dx_descriptor_handle createBufferUAV(dx_buffer& buffer, uint32 index, buffer_range bufferRange = {});
	dx_descriptor_handle pushBufferUAV(dx_buffer& buffer, buffer_range bufferRange = {});

	dx_descriptor_handle createRaytracingAccelerationStructureSRV(dx_buffer& tlas, uint32 index);
	dx_descriptor_handle pushRaytracingAccelerationStructureSRV(dx_buffer& tlas);
};

struct dx_cbv_srv_uav_descriptor_heap : dx_descriptor_range
{

};

struct dx_descriptor_page
{
	com<ID3D12DescriptorHeap> descriptorHeap;
	dx_descriptor_handle base;
	uint32 usedDescriptors;
	uint32 maxNumDescriptors;
	uint32 descriptorHandleIncrementSize;

	dx_descriptor_page* next;
};

struct dx_frame_descriptor_allocator
{
	dx_device device;

	dx_descriptor_page* usedPages[NUM_BUFFERED_FRAMES];
	dx_descriptor_page* freePages;
	uint32 currentFrame;

	thread_mutex mutex;

	void newFrame(uint32 bufferedFrameID);
	dx_descriptor_range allocateContiguousDescriptorRange(uint32 count);
};

struct dx_rtv_descriptor_heap : dx_descriptor_heap
{
	volatile uint32 pushIndex;

	dx_descriptor_handle pushRenderTargetView(dx_texture& texture);
	dx_descriptor_handle createRenderTargetView(dx_texture& texture, dx_descriptor_handle index);
};

struct dx_dsv_descriptor_heap : dx_descriptor_heap
{
	volatile uint32 pushIndex;

	dx_descriptor_handle pushDepthStencilView(dx_texture& texture);
	dx_descriptor_handle createDepthStencilView(dx_texture& texture, dx_descriptor_handle index);
};

static dx_descriptor_handle getHandle(dx_descriptor_heap& descriptorHeap, uint32 index)
{
	assert(index < descriptorHeap.maxNumDescriptors);

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(descriptorHeap.base.gpuHandle, index, descriptorHeap.descriptorHandleIncrementSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(descriptorHeap.base.cpuHandle, index, descriptorHeap.descriptorHandleIncrementSize);

	return { cpuHandle, gpuHandle };
}

static dx_descriptor_handle getHandle(dx_descriptor_range& range, uint32 index)
{
	assert(index < range.maxNumDescriptors);

	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(range.base.gpuHandle, index, range.descriptorHandleIncrementSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(range.base.cpuHandle, index, range.descriptorHandleIncrementSize);

	return { cpuHandle, gpuHandle };
}

struct dx_texture
{
	dx_resource resource;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;

	dx_descriptor_handle rtvHandles;
	dx_descriptor_handle dsvHandle;
};

struct dx_buffer
{
	dx_resource resource;

	uint32 elementSize;
	uint32 elementCount;
	uint32 totalSize;

	D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress;
};

struct dx_vertex_buffer : dx_buffer
{
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_index_buffer : dx_buffer
{
	D3D12_INDEX_BUFFER_VIEW view;
};

struct dx_mesh
{
	dx_vertex_buffer vertexBuffer;
	dx_index_buffer indexBuffer;
};

struct submesh_info
{
	uint32 numTriangles;
	uint32 firstTriangle;
	uint32 baseVertex;
	uint32 numVertices;
};

struct dx_submesh
{
	dx_vertex_buffer vertexBuffer;
	dx_index_buffer indexBuffer;
	uint32 numTriangles;
	uint32 firstTriangle;
	uint32 baseVertex;
};

struct dx_render_target
{
	dx_resource colorAttachments[8];
	dx_resource depthStencilAttachment;

	uint32 numAttachments;
	uint32 width;
	uint32 height;
	D3D12_VIEWPORT viewport;

	D3D12_RT_FORMAT_ARRAY renderTargetFormat;
	DXGI_FORMAT depthStencilFormat;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8];
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;


	uint32 pushColorAttachment(dx_texture& texture);
	void pushDepthStencilAttachment(dx_texture& texture);
	void resize(uint32 width, uint32 height); // This does NOT resize the textures, only updates the width, height and viewport variables.
};

DXGI_FORMAT getIndexBufferFormat(uint32 elementSize);

void* mapBuffer(dx_buffer& buffer);
void unmapBuffer(dx_buffer& buffer);
void uploadBufferData(dx_context* context, dx_buffer& buffer, const void* bufferData);
void updateBufferDataRange(dx_context* context, dx_buffer& buffer, const void* data, uint32 offset, uint32 size);

dx_buffer createBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
dx_buffer createUploadBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false);
dx_vertex_buffer createVertexBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false);
dx_vertex_buffer createUploadVertexBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false);
dx_index_buffer createIndexBuffer(dx_context* context, uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess);

dx_texture createTexture(dx_context* context, D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
dx_texture createTexture(dx_context* context, const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
dx_texture createDepthTexture(dx_context* context, uint32 width, uint32 height, DXGI_FORMAT format);
void resizeTexture(dx_context* context, dx_texture& texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

dx_root_signature createRootSignature(dx_context* context, const D3D12_ROOT_SIGNATURE_DESC1& desc);
dx_root_signature createRootSignature(dx_context* context, CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags);
dx_root_signature createRootSignature(dx_context* context, const D3D12_ROOT_SIGNATURE_DESC& desc);
dx_root_signature createRootSignature(dx_context* context, CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags);
dx_command_signature createCommandSignature(dx_context* context, dx_root_signature rootSignature, const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc);
dx_command_signature createCommandSignature(dx_context* context, dx_root_signature rootSignature, D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs, uint32 numArgumentDescs, uint32 commandStructureSize);

dx_cbv_srv_uav_descriptor_heap createDescriptorHeap(dx_context* context, uint32 numDescriptors, bool shaderVisible = true);
dx_rtv_descriptor_heap createRTVDescriptorAllocator(dx_context* context, uint32 numDescriptors);
dx_dsv_descriptor_heap createDSVDescriptorAllocator(dx_context* context, uint32 numDescriptors);

dx_frame_descriptor_allocator createFrameDescriptorAllocator(dx_context* context);

dx_submesh createSubmesh(dx_mesh& mesh, submesh_info info);
dx_submesh createSubmesh(dx_mesh& mesh);

static bool isUAVCompatibleFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SINT:
		return true;
	default:
		return false;
	}
}

static bool isSRGBFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

static bool isBGRFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return true;
	default:
		return false;
	}
}

static bool isDepthFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_D16_UNORM:
		return true;
	default:
		return false;
	}
}

static bool isTypelessFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC7_TYPELESS:
		return true;
	}
	return false;
}

static DXGI_FORMAT getTypelessFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT typelessFormat = format;

	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32B32A32_TYPELESS;
		break;
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32B32_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
		typelessFormat = DXGI_FORMAT_R16G16B16A16_TYPELESS;
		break;
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		typelessFormat = DXGI_FORMAT_R32G32_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		typelessFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
		break;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		typelessFormat = DXGI_FORMAT_R24G8_TYPELESS;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
		typelessFormat = DXGI_FORMAT_R10G10B10A2_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
		typelessFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
		typelessFormat = DXGI_FORMAT_R16G16_TYPELESS;
		break;
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
		typelessFormat = DXGI_FORMAT_R32_TYPELESS;
		break;
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		typelessFormat = DXGI_FORMAT_R8G8_TYPELESS;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
		typelessFormat = DXGI_FORMAT_R16_TYPELESS;
		break;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
		typelessFormat = DXGI_FORMAT_R8_TYPELESS;
		break;
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC1_TYPELESS;
		break;
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC2_TYPELESS;
		break;
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC3_TYPELESS;
		break;
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		typelessFormat = DXGI_FORMAT_BC4_TYPELESS;
		break;
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
		typelessFormat = DXGI_FORMAT_BC5_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_B8G8R8X8_TYPELESS;
		break;
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
		typelessFormat = DXGI_FORMAT_BC6H_TYPELESS;
		break;
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		typelessFormat = DXGI_FORMAT_BC7_TYPELESS;
		break;
	}

	return typelessFormat;
}

static DXGI_FORMAT getSRGBFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT srgbFormat = format;
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		srgbFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC1_UNORM:
		srgbFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC2_UNORM:
		srgbFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC3_UNORM:
		srgbFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		srgbFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		srgbFormat = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
		break;
	case DXGI_FORMAT_BC7_UNORM:
		srgbFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
		break;
	}

	return srgbFormat;
}

static DXGI_FORMAT getUAVCompatibleFormat(DXGI_FORMAT format)
{
	DXGI_FORMAT uavFormat = format;

	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		uavFormat = DXGI_FORMAT_R32_FLOAT;
		break;
	}

	return uavFormat;
}

static DXGI_FORMAT getDepthFormatFromTypeless(DXGI_FORMAT format)
{
	// Incomplete list.
	switch (format)
	{
	case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_D16_UNORM;
	}

	return format;
}

static DXGI_FORMAT getReadFormatFromTypeless(DXGI_FORMAT format)
{
	// Incomplete list.
	switch (format)
	{
	case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
	}

	return format;
}

static uint32 getFormatSize(DXGI_FORMAT format)
{
	uint32 size = 0;

	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		size = 4 * 4;
		break;
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
	case DXGI_FORMAT_R32G32B32_TYPELESS:
		size = 3 * 4;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		size = 4 * 2;
		break;
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		size = 2 * 4;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		size = 4;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		size = 4;
		break;
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
		size = 2 * 2;
		break;
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
		size = 4;
		break;
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R8G8_TYPELESS:
		size = 2;
		break;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
		size = 2;
		break;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_R8_TYPELESS:
		size = 1;
		break;
		size = 4;
		break;

	default:
		assert(false); // Compressed format.
	}

	return size;
}

static bool checkFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT1 support)
{
	return (formatSupport.Support1 & support) != 0;
}

static bool checkFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT2 support)
{
	return (formatSupport.Support2 & support) != 0;
}

static bool formatSupportsRTV(dx_texture& texture)
{
	return checkFormatSupport(texture.formatSupport, D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
}

static bool formatSupportsDSV(dx_texture& texture)
{
	return checkFormatSupport(texture.formatSupport, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
}

static bool formatSupportsSRV(dx_texture& texture)
{
	return checkFormatSupport(texture.formatSupport, D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
}

static bool formatSupportsUAV(dx_texture& texture)
{
	return checkFormatSupport(texture.formatSupport, D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
		checkFormatSupport(texture.formatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
		checkFormatSupport(texture.formatSupport, D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
}

static dx_device getDevice(com<ID3D12DeviceChild> object)
{
	dx_device device = 0;
	object->GetDevice(IID_ID3D12Device, &device);
	return device;
}
