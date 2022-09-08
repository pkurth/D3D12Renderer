#include "pch.h"
#include "debug_visualization.h"
#include "core/cpu_profiling.h"
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






static D3D12_INPUT_ELEMENT_DESC inputLayout_position_color[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};


void debug_unlit_line_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_color)
		.renderTargets(ldrFormat, depthStencilFormat)
		.primitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

	unlitLinePipeline = createReloadablePipeline(desc, { "flat_unlit_vs", "flat_unlit_ps" });
}

static std::tuple<dx_dynamic_vertex_buffer, dx_dynamic_index_buffer> getWireRing()
{
	const uint32 numSegments = 32;

	uint32 numLines = numSegments;
	uint32 numVertices = numSegments;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(position_color), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numLines * 2);

	position_color* vertices = (position_color*)vertexPtr;
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

	return { vb, ib };
}

static std::tuple<dx_dynamic_vertex_buffer, dx_dynamic_index_buffer> getWireCapsuleCrossSection(float length, float radius)
{
	const uint32 numSegmentsPerHalf = 16;

	uint32 numVertices = numSegmentsPerHalf * 2 + 2;
	uint32 numLines = numVertices;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(position_color), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numLines * 2);

	position_color* verticesA = (position_color*)vertexPtr;
	position_color* verticesB = verticesA + (numSegmentsPerHalf + 1);
	indexed_line16* lines = (indexed_line16*)indexPtr;


	float deltaRot = M_PI / numSegmentsPerHalf;
	float rot = 0.f;
	float halfLength = length * 0.5f;
	for (uint32 i = 0; i < numSegmentsPerHalf + 1; ++i)
	{
		float x = cos(rot), y = sin(rot);
		*verticesA++ =  vec3(radius * x, radius * y + halfLength, 0.f);
		*verticesB++ = -vec3(radius * x, radius * y + halfLength, 0.f);
		rot += deltaRot;
	}

	for (uint16 i = 0; i < numLines; ++i)
	{
		uint16 next = i + 1;
		if (i == numLines - 1)
		{
			next = 0;
		}
		*lines++ = { i, next };
	}

	return { vb, ib };
}

void renderWireDebug(const mat4& transform, const dx_dynamic_vertex_buffer& vb, const dx_dynamic_index_buffer& ib, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	assert(vb.view.StrideInBytes == sizeof(position_color));

	submesh_info sm;
	sm.baseVertex = 0;
	sm.numVertices = vb.view.SizeInBytes / vb.view.StrideInBytes;
	sm.firstIndex = 0;
	sm.numIndices = ib.view.SizeInBytes / getFormatSize(ib.view.Format);

	if (overlay)
	{
		renderPass->renderOverlay<debug_unlit_line_pipeline>(transform, dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
	}
	else
	{
		renderPass->renderObject<debug_unlit_line_pipeline>(transform, dx_vertex_buffer_group_view(vb), ib, sm, debug_line_material{ color });
	}
}

void renderLine(vec3 positionA, vec3 positionB, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render line");

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(position_color), 2);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 2);

	position_color* vertices = (position_color*)vertexPtr;
	indexed_line16* lines = (indexed_line16*)indexPtr;

	*vertices++ = positionA;
	*vertices++ = positionB;

	*lines++ = { 0, 1 };

	renderWireDebug(mat4::identity, vb, ib, color, renderPass, overlay);
}

void renderWireSphere(vec3 position, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire sphere");

	auto [vb, ib] = getWireRing();

	renderWireDebug(createModelMatrix(position, quat::identity, radius), vb, ib, color, renderPass, overlay);
	renderWireDebug(createModelMatrix(position, quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), radius), vb, ib, color, renderPass, overlay);
	renderWireDebug(createModelMatrix(position, quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), radius), vb, ib, color, renderPass, overlay);
}

void renderWireCapsule(vec3 positionA, vec3 positionB, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire capsule");

	vec3 center = 0.5f * (positionA + positionB);
	quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), positionA - positionB);
	auto [vb, ib] = getWireCapsuleCrossSection(length(positionA - positionB), radius);

	renderWireDebug(createModelMatrix(center, rotation * quat::identity, 1.f), vb, ib, color, renderPass, overlay);
	renderWireDebug(createModelMatrix(center, rotation * quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), 1.f), vb, ib, color, renderPass, overlay);
}

void renderWireCone(vec3 position, vec3 direction, float distance, float angle, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire cone");

	const uint32 numSegments = 32;
	const uint32 numConeLines = 8;
	static_assert(numSegments % numConeLines == 0, "");

	uint32 numLines = numConeLines + numSegments;
	uint32 numVertices = 1 + numSegments;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(position_color), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numLines * 2);

	position_color* vertices = (position_color*)vertexPtr;
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

	renderWireDebug(mat4::identity, vb, ib, color, renderPass, overlay);
}

void renderWireBox(vec3 position, vec3 radius, quat rotation, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire box");

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(position_color), 8);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 12 * 2);

	position_color* vertices = (position_color*)vertexPtr;
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

	renderWireDebug(createModelMatrix(position, rotation, radius), vb, ib, color, renderPass, overlay);
}

void renderCameraFrustum(const render_camera& frustum, vec4 color, ldr_render_pass* renderPass, float alternativeFarPlane, bool overlay)
{
	CPU_PROFILE_BLOCK("Render camera frustum");

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(position_color), 8);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 12 * 2);

	position_color* vertices = (position_color*)vertexPtr;
	camera_frustum_corners corners = frustum.getWorldSpaceFrustumCorners(alternativeFarPlane);
	for (uint32 i = 0; i < 8; ++i)
	{
		*vertices++ = corners.corners[i];
	}

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

	renderWireDebug(mat4::identity, vb, ib, color, renderPass, overlay);
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
