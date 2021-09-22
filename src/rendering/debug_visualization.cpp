#include "pch.h"
#include "debug_visualization.h"
#include "render_utils.h"
#include "render_resources.h"
#include "transform.hlsli"

static dx_pipeline simplePipeline;
static dx_pipeline unlitPipeline;
static dx_pipeline unlitLinePipeline;



void debug_simple_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv_normal)
		.renderTargets(ldrFormat, depthStencilFormat);

	simplePipeline = createReloadablePipeline(desc, { "flat_simple_textured_vs", "flat_simple_textured_ps" });
}

PIPELINE_SETUP_IMPL(debug_simple_pipeline)
{
	cl->setPipelineState(*simplePipeline.pipeline);
	cl->setGraphicsRootSignature(*simplePipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setGraphicsDynamicConstantBuffer(FLAT_SIMPLE_RS_CAMERA, materialInfo.cameraCBV);
}

PIPELINE_RENDER_IMPL(debug_simple_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_SIMPLE_RS_TRANFORM, transform_cb{ viewProj * rc.transform, rc.transform });
	cl->setGraphics32BitConstants(FLAT_SIMPLE_RS_CB, visualization_textured_cb{ rc.material.color, rc.material.uv0, rc.material.uv1 });
	cl->setDescriptorHeapSRV(FLAT_SIMPLE_RS_TEXTURE, 0, rc.material.texture ? rc.material.texture : render_resources::whiteTexture);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numIndices, 1, rc.submesh.firstIndex, rc.submesh.baseVertex, 0);
}





void debug_unlit_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv)
		.renderTargets(ldrFormat, depthStencilFormat);

	unlitPipeline = createReloadablePipeline(desc, { "flat_unlit_textured_vs", "flat_unlit_textured_ps" });
}

PIPELINE_SETUP_IMPL(debug_unlit_pipeline)
{
	cl->setPipelineState(*unlitPipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(debug_unlit_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_TRANFORM, viewProj * rc.transform);
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_CB, visualization_textured_cb{ rc.material.color, rc.material.uv0, rc.material.uv1 });
	cl->setDescriptorHeapSRV(FLAT_UNLIT_RS_TEXTURE, 0, rc.material.texture ? rc.material.texture : render_resources::whiteTexture);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numIndices, 1, rc.submesh.firstIndex, rc.submesh.baseVertex, 0);
}







void debug_unlit_line_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(ldrFormat, depthStencilFormat)
		.primitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

	unlitLinePipeline = createReloadablePipeline(desc, { "flat_unlit_vs", "flat_unlit_ps" });
}

static std::tuple<dx_dynamic_vertex_buffer, dx_dynamic_index_buffer, submesh_info> getWireRing()
{
	const uint32 numSegments = 32;

	uint32 numLines = numSegments;
	uint32 numVertices = numSegments;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numLines * 2);

	vec3* vertices = (vec3*)vertexPtr;
	indexed_line16* lines = (indexed_line16*)indexPtr;

	float deltaRot = M_TAU / numSegments;
	float rot = 0.f;
	for (uint32 i = 0; i < numSegments; ++i)
	{
		*vertices++ = vec3(cos(rot), sin(rot), 0.f);
		rot += deltaRot;
	}

	for (uint16 i = 0; i < numSegments; ++i)
	{
		uint16 next = i + 1;
		if (i == numSegments - 1)
		{
			next = 0;
		}
		*lines++ = { i, next };
	}

	submesh_info sm;
	sm.baseVertex = 0;
	sm.numVertices = numVertices;
	sm.firstIndex = 0;
	sm.numIndices = numSegments * 2;

	return { vb, ib, sm };
}

void renderWireSphere(vec3 position, float radius, vec4 color, ldr_render_pass* renderPass)
{
	auto [vb, ib, sm] = getWireRing();

	renderPass->renderObject<debug_unlit_line_pipeline>(createModelMatrix(position, quat::identity, radius), dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
	renderPass->renderObject<debug_unlit_line_pipeline>(createModelMatrix(position, quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), radius), dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
	renderPass->renderObject<debug_unlit_line_pipeline>(createModelMatrix(position, quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), radius), dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
}

void renderWireCone(vec3 position, vec3 direction, float distance, float angle, vec4 color, ldr_render_pass* renderPass)
{
	const uint32 numSegments = 32;
	const uint32 numConeLines = 8;
	static_assert(numSegments % numConeLines == 0, "");

	uint32 numLines = numConeLines + numSegments;
	uint32 numVertices = 1 + numSegments;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numLines * 2);

	vec3* vertices = (vec3*)vertexPtr;
	indexed_line16* lines = (indexed_line16*)indexPtr;

	float halfAngle = angle * 0.5f;
	float axisLength = tan(halfAngle);

	vec3 xAxis, yAxis;
	getTangents(direction, xAxis, yAxis);
	vec3 zAxis = direction * distance;
	xAxis *= distance * axisLength;
	yAxis *= distance * axisLength;

	*vertices++ = position;

	float deltaRot = M_TAU / numSegments;
	float rot = 0.f;
	for (uint32 i = 0; i < numSegments; ++i)
	{
		*vertices++ = position + zAxis + xAxis * cos(rot) + yAxis * sin(rot);
		rot += deltaRot;
	}

	const uint16 step = numSegments / numConeLines;

	for (uint16 i = 0; i < numConeLines; ++i)
	{
		uint16 next = 1u + i * step;
		*lines++ = { 0, next };
	}

	for (uint16 i = 0; i < numSegments; ++i)
	{
		uint16 cur = i + 1;
		uint16 next = i + 2;
		if (i == numSegments - 1)
		{
			next = 1;
		}
		*lines++ = { cur, next };
	}

	submesh_info sm;
	sm.baseVertex = 0;
	sm.numVertices = numVertices;
	sm.firstIndex = 0;
	sm.numIndices = numLines * 2;

	renderPass->renderObject<debug_unlit_line_pipeline>(mat4::identity, dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
}

void renderWireBox(vec3 position, vec3 radius, quat rotation, vec4 color, ldr_render_pass* renderPass)
{
	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), 8);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 12 * 2);

	vec3* vertices = (vec3*)vertexPtr;
	*vertices++ = vec3(-1.f, 1.f, -1.f);
	*vertices++ = vec3(1.f, 1.f, -1.f);
	*vertices++ = vec3(-1.f, -1.f, -1.f);
	*vertices++ = vec3(1.f, -1.f, -1.f);
	*vertices++ = vec3(-1.f, 1.f, 1.f);
	*vertices++ = vec3(1.f, 1.f, 1.f);
	*vertices++ = vec3(-1.f, -1.f, 1.f);
	*vertices++ = vec3(1.f, -1.f, 1.f);

	indexed_line16* lines = (indexed_line16*)indexPtr;
	*lines++ = { 0, 1 };
	*lines++ = { 1, 3 };
	*lines++ = { 3, 2 };
	*lines++ = { 2, 0 };

	*lines++ = { 4, 5 };
	*lines++ = { 5, 7 };
	*lines++ = { 7, 6 };
	*lines++ = { 6, 4 };

	*lines++ = { 0, 4 };
	*lines++ = { 1, 5 };
	*lines++ = { 2, 6 };
	*lines++ = { 3, 7 };

	submesh_info sm;
	sm.baseVertex = 0;
	sm.numVertices = 8;
	sm.firstIndex = 0;
	sm.numIndices = 12 * 2;
	
	renderPass->renderObject<debug_unlit_line_pipeline>(createModelMatrix(position, rotation, radius), dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
}

void renderCameraFrustum(const render_camera& frustum, vec4 color, ldr_render_pass* renderPass, float alternativeFarPlane)
{
	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), 8);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 12 * 2);

	camera_frustum_corners corners = frustum.getWorldSpaceFrustumCorners(alternativeFarPlane);
	memcpy(vertexPtr, corners.corners, sizeof(vec3) * 8);

	indexed_line16* lines = (indexed_line16*)indexPtr;
	*lines++ = { 0, 1 };
	*lines++ = { 1, 3 };
	*lines++ = { 3, 2 };
	*lines++ = { 2, 0 };

	*lines++ = { 4, 5 };
	*lines++ = { 5, 7 };
	*lines++ = { 7, 6 };
	*lines++ = { 6, 4 };

	*lines++ = { 0, 4 };
	*lines++ = { 1, 5 };
	*lines++ = { 2, 6 };
	*lines++ = { 3, 7 };

	submesh_info sm;
	sm.baseVertex = 0;
	sm.numVertices = 8;
	sm.firstIndex = 0;
	sm.numIndices = 12 * 2;

	renderPass->renderObject<debug_unlit_line_pipeline>(mat4::identity, dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
}

PIPELINE_SETUP_IMPL(debug_unlit_line_pipeline)
{
	cl->setPipelineState(*unlitLinePipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitLinePipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

PIPELINE_RENDER_IMPL(debug_unlit_line_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_TRANFORM, viewProj * rc.transform);
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_CB, rc.material.color);
	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(rc.submesh.numIndices, 1, rc.submesh.firstIndex, rc.submesh.baseVertex, 0);
}
