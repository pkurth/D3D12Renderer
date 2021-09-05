#include "pch.h"
#include "debug_visualization.h"
#include "render_utils.h"
#include "render_resources.h"
#include "transform.hlsli"

static dx_pipeline simplePipeline;
static dx_pipeline unlitPipeline;



void debug_simple_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv_normal)
		.renderTargets(ldrFormat, depthStencilFormat)
		;

	simplePipeline = createReloadablePipeline(desc, { "flat_simple_vs", "flat_simple_ps" });
}

PIPELINE_SETUP_IMPL(debug_simple_pipeline)
{
	cl->setPipelineState(*simplePipeline.pipeline);
	cl->setGraphicsRootSignature(*simplePipeline.rootSignature);

	cl->setGraphicsDynamicConstantBuffer(FLAT_SIMPLE_RS_CAMERA, materialInfo.cameraCBV);
}

PIPELINE_RENDER_IMPL(debug_simple_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_SIMPLE_RS_TRANFORM, transform_cb{ viewProj * rc.transform, rc.transform });
	cl->setGraphics32BitConstants(FLAT_SIMPLE_RS_CB, visualization_cb{ rc.material.color, rc.material.uv0, rc.material.uv1 });
	cl->setDescriptorHeapSRV(FLAT_SIMPLE_RS_TEXTURE, 0, rc.material.texture ? rc.material.texture : render_resources::whiteTexture);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numTriangles * 3, 1, rc.submesh.firstTriangle * 3, rc.submesh.baseVertex, 0);
}





void debug_unlit_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv)
		.renderTargets(ldrFormat, depthStencilFormat)
		;

	unlitPipeline = createReloadablePipeline(desc, { "flat_unlit_vs", "flat_unlit_ps" });
}

PIPELINE_SETUP_IMPL(debug_unlit_pipeline)
{
	cl->setPipelineState(*unlitPipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitPipeline.rootSignature);
}

PIPELINE_RENDER_IMPL(debug_unlit_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_TRANFORM, viewProj * rc.transform);
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_CB, visualization_cb{ rc.material.color, rc.material.uv0, rc.material.uv1 });
	cl->setDescriptorHeapSRV(FLAT_UNLIT_RS_TEXTURE, 0, rc.material.texture ? rc.material.texture : render_resources::whiteTexture);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numTriangles * 3, 1, rc.submesh.firstTriangle * 3, rc.submesh.baseVertex, 0);
}

