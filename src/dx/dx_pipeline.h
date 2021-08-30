#pragma once

#include "dx.h"
#include "dx_context.h"

#define CREATE_GRAPHICS_PIPELINE dx_graphics_pipeline_generator()

#define UNBOUNDED_DESCRIPTOR_RANGE -1




struct dx_root_signature
{
	com<ID3D12RootSignature> rootSignature;
	uint32* descriptorTableSizes;
	uint32 numDescriptorTables;
	uint32 tableRootParameterMask;
	uint32 totalNumParameters;
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





struct dx_graphics_pipeline_generator
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;

	operator const D3D12_GRAPHICS_PIPELINE_STATE_DESC& () const
	{
		return desc;
	}

	dx_graphics_pipeline_generator()
	{
		desc = {};
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.RasterizerState.FrontCounterClockwise = true;
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleDesc = { 1, 0 };
		desc.SampleMask = 0xFFFFFFFF;
	}

	dx_graphics_pipeline_generator& rootSignature(const dx_root_signature& rootSignature)
	{
		desc.pRootSignature = rootSignature.rootSignature.Get();
		return *this;
	}

	dx_graphics_pipeline_generator& vs(dx_blob shader)
	{
		desc.VS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	dx_graphics_pipeline_generator& ps(dx_blob shader)
	{
		desc.PS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	dx_graphics_pipeline_generator& gs(dx_blob shader)
	{
		desc.GS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	dx_graphics_pipeline_generator& ds(dx_blob shader)
	{
		desc.DS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	dx_graphics_pipeline_generator& hs(dx_blob shader)
	{
		desc.HS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	dx_graphics_pipeline_generator& alphaToCoverage(bool enable)
	{
		desc.BlendState.AlphaToCoverageEnable = enable;
		return *this;
	}

	dx_graphics_pipeline_generator& independentRenderTargetBlending(bool enable)
	{
		desc.BlendState.IndependentBlendEnable = enable;
		return *this;
	}

	dx_graphics_pipeline_generator& blendState(uint32 renderTargetIndex, D3D12_BLEND srcBlend, D3D12_BLEND destBlend, D3D12_BLEND_OP op)
	{
		assert(!desc.BlendState.RenderTarget[renderTargetIndex].LogicOpEnable);
		desc.BlendState.RenderTarget[renderTargetIndex].BlendEnable = true;
		desc.BlendState.RenderTarget[renderTargetIndex].SrcBlend = srcBlend;
		desc.BlendState.RenderTarget[renderTargetIndex].DestBlend = destBlend;
		desc.BlendState.RenderTarget[renderTargetIndex].BlendOp = op;
		return *this;
	}

	dx_graphics_pipeline_generator& alphaBlending(uint32 renderTargetIndex)
	{
		return blendState(renderTargetIndex, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD);
	}

	dx_graphics_pipeline_generator& additiveBlending(uint32 renderTargetIndex)
	{
		return blendState(renderTargetIndex, D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD);
	}

	dx_graphics_pipeline_generator& wireframe()
	{
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		return *this;
	}

	dx_graphics_pipeline_generator& cullFrontFaces()
	{
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		return *this;
	}

	dx_graphics_pipeline_generator& cullingOff()
	{
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return *this;
	}

	dx_graphics_pipeline_generator& rasterizeClockwise()
	{
		desc.RasterizerState.FrontCounterClockwise = false;
		return *this;
	}

	dx_graphics_pipeline_generator& depthBias(int bias = D3D12_DEFAULT_DEPTH_BIAS, float biasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP, float slopeScaledBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS)
	{
		desc.RasterizerState.DepthBias = bias;
		desc.RasterizerState.DepthBiasClamp = biasClamp;
		desc.RasterizerState.SlopeScaledDepthBias = slopeScaledBias;
		return *this;
	}

	dx_graphics_pipeline_generator& antialiasing(bool multisampling = false, bool antialiasedlines = false, uint32 forcedSampleCount = 0)
	{
		desc.RasterizerState.MultisampleEnable = multisampling;
		desc.RasterizerState.AntialiasedLineEnable = antialiasedlines;
		desc.RasterizerState.ForcedSampleCount = forcedSampleCount;
		return *this;
	}

	dx_graphics_pipeline_generator& rasterizeConvervative()
	{
		desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
		return *this;
	}

	dx_graphics_pipeline_generator& depthSettings(bool depthTest = true, bool depthWrite = true, D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS)
	{
		desc.DepthStencilState.DepthEnable = depthTest;
		desc.DepthStencilState.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthStencilState.DepthFunc = func;
		return *this;
	}

	dx_graphics_pipeline_generator& stencilSettings(
		D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_ALWAYS,
		D3D12_STENCIL_OP onPass = D3D12_STENCIL_OP_KEEP, 
		D3D12_STENCIL_OP onStencilPassAndDepthFail = D3D12_STENCIL_OP_KEEP, 
		D3D12_STENCIL_OP onFail = D3D12_STENCIL_OP_KEEP, 
		uint8 readMask = D3D12_DEFAULT_STENCIL_READ_MASK, uint8 writeMask = D3D12_DEFAULT_STENCIL_WRITE_MASK)
	{
		desc.DepthStencilState.StencilEnable = true;
		desc.DepthStencilState.StencilReadMask = readMask;
		desc.DepthStencilState.StencilWriteMask = writeMask;

		desc.DepthStencilState.FrontFace.StencilFailOp = onFail;
		desc.DepthStencilState.FrontFace.StencilDepthFailOp = onStencilPassAndDepthFail;
		desc.DepthStencilState.FrontFace.StencilPassOp = onPass;
		desc.DepthStencilState.FrontFace.StencilFunc = func;

		desc.DepthStencilState.BackFace.StencilFailOp = onFail;
		desc.DepthStencilState.BackFace.StencilDepthFailOp = onStencilPassAndDepthFail;
		desc.DepthStencilState.BackFace.StencilPassOp = onPass;
		desc.DepthStencilState.BackFace.StencilFunc = func;
		return *this;
	}

	template <uint32 numElements>
	dx_graphics_pipeline_generator& inputLayout(D3D12_INPUT_ELEMENT_DESC (&elements)[numElements])
	{
		desc.InputLayout = { elements, numElements };
		return *this;
	}

	dx_graphics_pipeline_generator& inputLayout(D3D12_INPUT_ELEMENT_DESC* elements, uint32 numElements)
	{
		desc.InputLayout = { elements, numElements };
		return *this;
	}

	dx_graphics_pipeline_generator& primitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
	{
		desc.PrimitiveTopologyType = type;
		return *this;
	}

	dx_graphics_pipeline_generator& renderTargets(DXGI_FORMAT rtFormat, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN)
	{
		return renderTargets(&rtFormat, 1, dsvFormat);
	}

	dx_graphics_pipeline_generator& renderTargets(const DXGI_FORMAT* rtFormats, uint32 numRenderTargets, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN)
	{
		memcpy(desc.RTVFormats, rtFormats, sizeof(DXGI_FORMAT) * numRenderTargets);
		desc.NumRenderTargets = numRenderTargets;
		desc.DSVFormat = dsvFormat;
		return *this;
	}

	dx_graphics_pipeline_generator& renderTargets(const D3D12_RT_FORMAT_ARRAY& rtFormats, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN)
	{
		return renderTargets(rtFormats.RTFormats, rtFormats.NumRenderTargets, dsvFormat);
	}

	dx_graphics_pipeline_generator& multisampling(uint32 count = 1, uint32 quality = 0)
	{
		desc.SampleDesc = { count, quality };
		return *this;
	}
};



struct root_descriptor_table : CD3DX12_ROOT_PARAMETER
{
	root_descriptor_table(uint32 numDescriptorRanges,
		const D3D12_DESCRIPTOR_RANGE* descriptorRanges,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsDescriptorTable(numDescriptorRanges, descriptorRanges, visibility);
	}
};

template <typename T>
struct root_constants : CD3DX12_ROOT_PARAMETER
{
	root_constants(uint32 shaderRegister,
		uint32 space = 0,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsConstants(sizeof(T) / 4, shaderRegister, space, visibility);
	}
};

struct root_cbv : CD3DX12_ROOT_PARAMETER
{
	root_cbv(uint32 shaderRegister,
		uint32 space = 0,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsConstantBufferView(shaderRegister, space, visibility);
	}
};

struct root_srv : CD3DX12_ROOT_PARAMETER
{
	root_srv(uint32 shaderRegister,
		uint32 space = 0,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsShaderResourceView(shaderRegister, space, visibility);
	}
};

struct root_uav : CD3DX12_ROOT_PARAMETER
{
	root_uav(uint32 shaderRegister,
		uint32 space = 0,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsUnorderedAccessView(shaderRegister, space, visibility);
	}
};




struct indirect_vertex_buffer : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_vertex_buffer(uint32 slot)
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
		VertexBuffer.Slot = slot;
	}
};

struct indirect_index_buffer : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_index_buffer()
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
	}
};

template <typename T>
struct indirect_root_constants : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_root_constants(uint32 rootParameterIndex, uint32 destOffsetIn32BitValues = 0)
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		Constant.Num32BitValuesToSet = sizeof(T) / 4;
		Constant.RootParameterIndex = rootParameterIndex;
		Constant.DestOffsetIn32BitValues = destOffsetIn32BitValues;
	}
};

struct indirect_cbv : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_cbv(uint32 rootParameterIndex)
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
		ConstantBufferView.RootParameterIndex = rootParameterIndex;
	}
};

struct indirect_srv : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_srv(uint32 rootParameterIndex)
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
		ConstantBufferView.RootParameterIndex = rootParameterIndex;
	}
};

struct indirect_uav : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_uav(uint32 rootParameterIndex)
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
		ConstantBufferView.RootParameterIndex = rootParameterIndex;
	}
};

struct indirect_draw : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_draw()
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	}
};

struct indirect_draw_indexed : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_draw_indexed()
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	}
};

struct indirect_dispatch : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_dispatch()
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	}
};

struct indirect_raytrace : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_raytrace()
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;
	}
};

struct indirect_mesh_shader : D3D12_INDIRECT_ARGUMENT_DESC
{
	indirect_mesh_shader()
	{
		Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
	}
};

struct dx_pipeline_stream_base
{
	virtual void setVertexShader(dx_blob blob) {}
	virtual void setPixelShader(dx_blob blob) {}
	virtual void setDomainShader(dx_blob blob) {}
	virtual void setHullShader(dx_blob blob) {}
	virtual void setGeometryShader(dx_blob blob) {}
	virtual void setMeshShader(dx_blob blob) {}
	virtual void setAmplificationShader(dx_blob blob) {}

	virtual void setRootSignature(dx_root_signature rs) = 0;
};


struct dx_pipeline
{
	dx_pipeline_state* pipeline;
	dx_root_signature* rootSignature;
};

union graphics_pipeline_files
{
	struct
	{
		const char* vs;
		const char* ps;
		const char* ds;
		const char* hs;
		const char* gs;
		const char* ms;
		const char* as;
	};
	const char* shaders[7] = {};
};

enum rs_file
{
	rs_in_vertex_shader,
	rs_in_pixel_shader,
	rs_in_domain_shader,
	rs_in_hull_shader,
	rs_in_geometry_shader,
	rs_in_mesh_shader,
	rs_in_amplification_shader,
};

dx_pipeline createReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files, dx_root_signature userRootSignature);
dx_pipeline createReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files, rs_file rootSignatureFile = rs_in_pixel_shader);

dx_pipeline createReloadablePipeline(const char* csFile, dx_root_signature userRootSignature);
dx_pipeline createReloadablePipeline(const char* csFile);


template <typename stream_t>
inline dx_pipeline createReloadablePipelineFromStream(const stream_t& stream, const graphics_pipeline_files& files, dx_root_signature userRootSignature)
{
	static_assert(std::is_base_of_v<dx_pipeline_stream_base, stream_t>, "Stream must inherit from dx_pipeline_stream_base.");

	stream_t* streamCopy = new stream_t(stream); // Dynamically allocated for permanent storage.
	D3D12_PIPELINE_STATE_STREAM_DESC desc = {
		sizeof(stream_t) - 8, (uint8*)streamCopy + 8 // Offset for vTable. This seems very broken. TODO: Verify that this always works.
	};

	dx_pipeline createReloadablePipeline(const D3D12_PIPELINE_STATE_STREAM_DESC& desc, dx_pipeline_stream_base* stream, const graphics_pipeline_files& files, dx_root_signature userRootSignature);
	return createReloadablePipeline(desc, streamCopy, files, userRootSignature);
}

template <typename stream_t>
inline dx_pipeline createReloadablePipelineFromStream(const stream_t& stream, const graphics_pipeline_files& files, rs_file rootSignatureFile = rs_in_pixel_shader)
{
	static_assert(std::is_base_of_v<dx_pipeline_stream_base, stream_t>, "Stream must inherit from dx_pipeline_stream_base.");

	stream_t* streamCopy = new stream_t(stream); // Dynamically allocated for permanent storage.
	D3D12_PIPELINE_STATE_STREAM_DESC desc = {
		sizeof(stream_t) - 8, (uint8*)streamCopy + 8 // Offset for vTable. This seems very broken. TODO: Verify that this always works.
	};

	dx_pipeline createReloadablePipeline(const D3D12_PIPELINE_STATE_STREAM_DESC& desc, dx_pipeline_stream_base* stream, const graphics_pipeline_files& files, rs_file rootSignatureFile);
	return createReloadablePipeline(desc, streamCopy, files, rootSignatureFile);
}

void createAllPendingReloadablePipelines();

void checkForChangedPipelines();

