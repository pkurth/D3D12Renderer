#pragma once

#include "dx.h"
#include "dx_descriptor.h"
#include "threading.h"

struct dx_texture;
struct dx_buffer;
struct dx_command_list;

#define UNBOUNDED_DESCRIPTOR_RANGE -1


struct dx_dynamic_constant_buffer
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;
	void* cpuPtr;
};

struct dx_dynamic_vertex_buffer
{
	D3D12_VERTEX_BUFFER_VIEW view;
};

struct dx_texture
{
	dx_resource resource;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;

	dx_cpu_descriptor_handle rtvHandles;
	dx_cpu_descriptor_handle dsvHandle;

	dx_cpu_descriptor_handle defaultSRV;
	dx_cpu_descriptor_handle defaultUAV;
};

struct dx_buffer
{
	dx_resource resource;

	uint32 elementSize;
	uint32 elementCount;
	uint32 totalSize;
	D3D12_HEAP_TYPE heapType;

	D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress;

	dx_cpu_descriptor_handle defaultSRV;
	dx_cpu_descriptor_handle defaultUAV;

	dx_cpu_descriptor_handle cpuClearUAV;
	dx_gpu_descriptor_handle gpuClearUAV;
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

struct dx_render_target
{
	uint32 numAttachments;
	uint32 width;
	uint32 height;
	D3D12_VIEWPORT viewport;

	D3D12_RT_FORMAT_ARRAY renderTargetFormat;
	DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_UNKNOWN;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8];
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;


	uint32 pushColorAttachment(dx_texture& texture);
	void pushDepthStencilAttachment(dx_texture& texture, uint32 arraySlice = 0);
	void notifyOnTextureResize(uint32 width, uint32 height); // This does NOT resize the textures, only updates the width, height and viewport variables.
};

DXGI_FORMAT getIndexBufferFormat(uint32 elementSize);

void* mapBuffer(dx_buffer& buffer);
void unmapBuffer(dx_buffer& buffer);
void uploadBufferData(dx_buffer& buffer, const void* bufferData);
void updateBufferDataRange(dx_buffer& buffer, const void* data, uint32 offset, uint32 size);

dx_buffer createBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
dx_buffer createUploadBuffer(uint32 elementSize, uint32 elementCount, void* data);
dx_vertex_buffer createVertexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
dx_vertex_buffer createUploadVertexBuffer(uint32 elementSize, uint32 elementCount, void* data);
dx_index_buffer createIndexBuffer(uint32 elementSize, uint32 elementCount, void* data, bool allowUnorderedAccess = false, bool allowClearing = false);
void resizeBuffer(dx_buffer& buffer, uint32 newElementCount, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
void freeBuffer(dx_buffer& buffer);

void uploadTextureSubresourceData(dx_texture& texture, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 firstSubresource, uint32 numSubresources);
dx_texture createTexture(D3D12_RESOURCE_DESC textureDesc, D3D12_SUBRESOURCE_DATA* subresourceData, uint32 numSubresources, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
dx_texture createTexture(const void* data, uint32 width, uint32 height, DXGI_FORMAT format, bool allowRenderTarget = false, bool allowUnorderedAccess = false, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
dx_texture createDepthTexture(uint32 width, uint32 height, DXGI_FORMAT format, uint32 arrayLength = 1, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE);
void resizeTexture(dx_texture& texture, uint32 newWidth, uint32 newHeight, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
void freeTexture(dx_texture& texture);

struct dx_root_signature
{
	com<ID3D12RootSignature> rootSignature;
	uint32* descriptorTableSizes;
	uint32 numDescriptorTables;
	uint32 tableRootParameterMask;
};

dx_root_signature createRootSignature(dx_blob rootSignatureBlob);
dx_root_signature createRootSignature(const wchar* path);
dx_root_signature createRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& desc);
dx_root_signature createRootSignature(CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags);
dx_root_signature createRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);
dx_root_signature createRootSignature(CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers,
	D3D12_ROOT_SIGNATURE_FLAGS flags);
dx_command_signature createCommandSignature(dx_root_signature rootSignature, const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc);
dx_root_signature createRootSignature(D3D12_ROOT_SIGNATURE_FLAGS flags);
void freeRootSignature(dx_root_signature& rs);

dx_command_signature createCommandSignature(dx_root_signature rootSignature, D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs, uint32 numArgumentDescs, uint32 commandStructureSize);



struct barrier_batcher
{
	barrier_batcher(dx_command_list* cl);
	~barrier_batcher() { submit(); }

	barrier_batcher& transition(dx_resource& res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to, uint32 subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	barrier_batcher& uav(dx_resource resource);
	barrier_batcher& aliasing(dx_resource before, dx_resource after);

	void submit();

	dx_command_list* cl;
	CD3DX12_RESOURCE_BARRIER barriers[16];
	uint32 numBarriers = 0;
};




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
