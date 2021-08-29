#include "pch.h"
#include "debug_visualization.h"
#include "render_utils.h"
#include "transform.hlsli"

static dx_pipeline flatSimplePipeline;
static dx_pipeline flatUnlitPipeline;



void debug_simple_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_normal)
		.renderTargets(ldrFormat, depthStencilFormat)
		;

	flatSimplePipeline = createReloadablePipeline(desc, { "flat_simple_vs", "flat_simple_ps" });
}

void debug_simple_pipeline::setupCommon(dx_command_list* cl, const common_material_info& materialInfo)
{
	cl->setPipelineState(*flatSimplePipeline.pipeline);
	cl->setGraphicsRootSignature(*flatSimplePipeline.rootSignature);

	cl->setGraphicsDynamicConstantBuffer(2, materialInfo.cameraCBV);
}

void debug_simple_pipeline::render(dx_command_list* cl, const mat4& viewProj, const default_render_command<debug_simple_pipeline>& rc)
{
	cl->setGraphics32BitConstants(0, transform_cb{ viewProj * rc.transform, rc.transform });
	cl->setGraphics32BitConstants(1, rc.material.color);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numTriangles * 3, 1, rc.submesh.firstTriangle * 3, rc.submesh.baseVertex, 0);
}





void debug_unlit_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(ldrFormat, depthStencilFormat)
		;

	flatUnlitPipeline = createReloadablePipeline(desc, { "flat_unlit_vs", "flat_unlit_ps" });
}

void debug_unlit_pipeline::setupCommon(dx_command_list* cl, const common_material_info& materialInfo)
{
	cl->setPipelineState(*flatUnlitPipeline.pipeline);
	cl->setGraphicsRootSignature(*flatUnlitPipeline.rootSignature);
}

void debug_unlit_pipeline::render(dx_command_list* cl, const mat4& viewProj, const default_render_command<debug_unlit_pipeline>& rc)
{
	cl->setGraphics32BitConstants(0, rc.transform);
	cl->setGraphics32BitConstants(1, rc.material.color);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numTriangles * 3, 1, rc.submesh.firstTriangle * 3, rc.submesh.baseVertex, 0);
}
