#include "pch.h"
#include "depth_prepass.h"
#include "render_utils.h"
#include "render_resources.h"

#include "dx/dx_command_list.h"

#include "depth_only_rs.hlsli"


static dx_pipeline defaultPipeline;
static dx_pipeline doubleSidedDefaultPipeline;
static dx_pipeline alphaCutoutPipeline;


static D3D12_INPUT_ELEMENT_DESC layout[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "PREV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC alphaCutoutLayout[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "PREV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 2, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};


void initializeDepthPrepassPipelines()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), depthStencilFormat)
		.inputLayout(layout);

	defaultPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);

	desc.cullingOff();
	doubleSidedDefaultPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);

	desc.inputLayout(alphaCutoutLayout);
	alphaCutoutPipeline = createReloadablePipeline(desc, { "depth_only_alpha_cutout_vs", "depth_only_alpha_cutout_ps" }, rs_in_vertex_shader);
}

static void setupDepthPrepassCommon(dx_command_list* cl, const dx_pipeline& pipeline, dx_dynamic_constant_buffer cameraCBV)
{
	cl->setPipelineState(*pipeline.pipeline);
	cl->setGraphicsRootSignature(*pipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setGraphicsDynamicConstantBuffer(DEPTH_ONLY_RS_CAMERA, cameraCBV);
}


PIPELINE_SETUP_IMPL(depth_prepass_pipeline::single_sided)
{
	setupDepthPrepassCommon(cl, defaultPipeline, common.cameraCBV);
}

PIPELINE_SETUP_IMPL(depth_prepass_pipeline::double_sided)
{
	setupDepthPrepassCommon(cl, doubleSidedDefaultPipeline, common.cameraCBV);
}

PIPELINE_SETUP_IMPL(depth_prepass_pipeline::alpha_cutout)
{
	setupDepthPrepassCommon(cl, alphaCutoutPipeline, common.cameraCBV);
}

static void render(dx_command_list* cl, const depth_prepass_data& data)
{
	cl->setRootGraphicsSRV(DEPTH_ONLY_RS_TRANSFORM, data.transformPtr);
	cl->setRootGraphicsSRV(DEPTH_ONLY_RS_PREV_FRAME_TRANSFORM, data.prevFrameTransformPtr);
	cl->setRootGraphicsSRV(DEPTH_ONLY_RS_OBJECT_ID, data.objectIDPtr);

	cl->setVertexBuffer(0, data.vertexBuffer.positions);
	cl->setVertexBuffer(1, data.prevFrameVertexBuffer);
	cl->setIndexBuffer(data.indexBuffer);
	cl->drawIndexed(data.submesh.numIndices, data.numInstances, data.submesh.firstIndex, data.submesh.baseVertex, 0);
}

DEPTH_ONLY_RENDER_IMPL(depth_prepass_pipeline, depth_prepass_data)
{
	::render(cl, rc.data);
}

DEPTH_ONLY_RENDER_IMPL(depth_prepass_pipeline::alpha_cutout, depth_prepass_data)
{
	cl->setDescriptorHeapSRV(DEPTH_ONLY_RS_ALPHA_TEXTURE, 0, rc.data.alphaCutoutTextureSRV ? rc.data.alphaCutoutTextureSRV : render_resources::whiteTexture->defaultSRV);
	cl->setVertexBuffer(2, rc.data.vertexBuffer.others);
	::render(cl, rc.data);
}













