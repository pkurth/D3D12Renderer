#include "pch.h"
#include "outline.h"

#include "render_pass.h"
#include "material.h"
#include "render_resources.h"
#include "render_algorithms.h"

#include "dx/dx_pipeline.h"

#include "outline_rs.hlsli"


static dx_pipeline outlinePipeline;

void initializeOutlinePipelines()
{
	auto markerDesc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(0, 0, depthStencilFormat)
		.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS,
			D3D12_STENCIL_OP_REPLACE,
			D3D12_STENCIL_OP_REPLACE,
			D3D12_STENCIL_OP_KEEP,
			D3D12_DEFAULT_STENCIL_READ_MASK,
			stencil_flag_selected_object) // Mark selected object.
		.depthSettings(false, false)
		.cullingOff(); // Since this is fairly light-weight, we only render double sided.

	outlinePipeline = createReloadablePipeline(markerDesc, { "outline_vs" }, rs_in_vertex_shader);
}

struct outline_render_data
{
	mat4 transform;
	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct outline_pipeline
{
	using render_data_t = outline_render_data;

	PIPELINE_SETUP_DECL
	{
		cl->setPipelineState(*outlinePipeline.pipeline);
		cl->setGraphicsRootSignature(*outlinePipeline.rootSignature);

		cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	PIPELINE_RENDER_DECL
	{
		cl->setGraphics32BitConstants(OUTLINE_RS_MVP, outline_marker_cb{ viewProj * rc.data.transform });

		cl->setVertexBuffer(0, rc.data.vertexBuffer);
		cl->setIndexBuffer(rc.data.indexBuffer);
		cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
	}
};

void renderOutline(ldr_render_pass* renderPass, const mat4& transform, dx_vertex_buffer_view vertexBuffer, dx_index_buffer_view indexBuffer, submesh_info submesh)
{
	outline_render_data data = {
		transform,
		vertexBuffer,
		indexBuffer,
		submesh,
	};

	renderPass->renderOutline<outline_pipeline>(data);
}

void renderOutline(ldr_render_pass* renderPass, const mat4& transform, dx_vertex_buffer_group_view vertexBuffer, dx_index_buffer_view indexBuffer, submesh_info submesh)
{
	renderOutline(renderPass, transform, vertexBuffer.positions, indexBuffer, submesh);
}
