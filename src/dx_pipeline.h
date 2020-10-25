#pragma once

#include "dx.h"
#include "dx_context.h"

#define CREATE_GRAPHICS_PIPELINE dx_graphics_pipeline_generator()
#define CREATE_COMPUTE_PIPELINE dx_compute_pipeline_generator()

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
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleDesc = { 1, 0 };
		desc.SampleMask = 0xFFFFFFFF;
	}

	dx_pipeline_state create(dx_context& dxContext)
	{
		dx_pipeline_state result;
		checkResult(dxContext.device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result)));
		return result;
	}

	dx_graphics_pipeline_generator& rootSignature(dx_root_signature rootSignature)
	{
		desc.pRootSignature = rootSignature.Get();
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

	dx_graphics_pipeline_generator& rasterizeCounterClockwise()
	{
		desc.RasterizerState.FrontCounterClockwise = true;
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

	dx_graphics_pipeline_generator& stencilSettings(bool stencilTest = true)
	{
		desc.DepthStencilState.StencilEnable = stencilTest;
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

	dx_graphics_pipeline_generator& renderTargets(DXGI_FORMAT* rtFormats, uint32 numRenderTargets, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN)
	{
		memcpy(desc.RTVFormats, rtFormats, sizeof(DXGI_FORMAT) * numRenderTargets);
		desc.NumRenderTargets = numRenderTargets;
		desc.DSVFormat = dsvFormat;
		return *this;
	}

	dx_graphics_pipeline_generator& renderTargets(D3D12_RT_FORMAT_ARRAY& rtFormats, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN)
	{
		return renderTargets(rtFormats.RTFormats, rtFormats.NumRenderTargets, dsvFormat);
	}

	dx_graphics_pipeline_generator& multisampling(uint32 count = 1, uint32 quality = 0)
	{
		desc.SampleDesc = { count, quality };
		return *this;
	}
};

struct dx_compute_pipeline_generator
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc;

	operator const D3D12_COMPUTE_PIPELINE_STATE_DESC& () const
	{
		return desc;
	}

	dx_compute_pipeline_generator()
	{
		desc = {};
	}

	dx_pipeline_state create(dx_context& dxContext)
	{
		dx_pipeline_state result;
		checkResult(dxContext.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&result)));
		return result;
	}

	dx_compute_pipeline_generator& rootSignature(dx_root_signature rootSignature)
	{
		desc.pRootSignature = rootSignature.Get();
		return *this;
	}

	dx_compute_pipeline_generator& cs(dx_blob shader)
	{
		desc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}
};



struct dx_pipeline
{
	dx_pipeline_state* pipeline;
	dx_root_signature* rootSignature;
};

struct graphics_pipeline_files
{
	std::string rs;
	std::string vs;
	std::string ps;
	std::string ds;
	std::string hs;
	std::string gs;
};

dx_pipeline createReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const graphics_pipeline_files& files,
	dx_root_signature userRootSignature = nullptr);
void createAllReloadablePipelines();

void checkForChangedPipelines();

