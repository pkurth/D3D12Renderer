#include "pch.h"
#include "dx_pipeline.h"
#include "dx_renderer.h"

static dx_pipeline meshShaderPipeline;
static ref<struct mesh_shader_material> meshShaderMaterial;

struct mesh_shader_material : material_base
{
	static void setupPipeline(dx_command_list* cl, const common_material_info& info)
	{
		cl->setPipelineState(*meshShaderPipeline.pipeline);
		cl->setGraphicsRootSignature(*meshShaderPipeline.rootSignature);
	}

	void prepareForRendering(dx_command_list* cl)
	{
	}
};

void initializeMeshShader()
{
	struct pipeline_state_stream : dx_pipeline_stream_base
	{
		// Will be set by reloader.
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_MS ms;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;

		// Initialized here.
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;

		void setRootSignature(dx_root_signature rs) override
		{
			rootSignature = rs.rootSignature.Get();
		}

		void setMeshShader(dx_blob blob) override 
		{
			ms = CD3DX12_SHADER_BYTECODE(blob.Get());
		}

		void setPixelShader(dx_blob blob) override
		{
			ps = CD3DX12_SHADER_BYTECODE(blob.Get());
		}
	};

	D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
	renderTargetFormat.NumRenderTargets = 1;
	renderTargetFormat.RTFormats[0] = dx_renderer::hdrFormat[0];

	auto defaultRasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//defaultRasterizerDesc.FrontCounterClockwise = TRUE; // Righthanded coordinate system.

	pipeline_state_stream stream;
	stream.inputLayout = { nullptr, 0 };
	stream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stream.dsvFormat = dx_renderer::hdrDepthStencilFormat;
	stream.rtvFormats = renderTargetFormat;
	stream.rasterizer = defaultRasterizerDesc;

	graphics_pipeline_files files = {};
	files.ms = "mesh_shader_ms";
	files.ps = "mesh_shader_ps";

	meshShaderPipeline = createReloadablePipelineFromStream(stream, files, rs_in_mesh_shader);

	meshShaderMaterial = make_ref<mesh_shader_material>();
}

void testRenderMeshShader(overlay_render_pass* overlayRenderPass)
{
	overlayRenderPass->renderObjectWithMeshShader(1, 1, 1,
		meshShaderMaterial,
		createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 1.f)
	);
}


