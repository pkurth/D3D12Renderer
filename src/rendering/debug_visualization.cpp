#include "pch.h"
#include "debug_visualization.h"
#include "core/cpu_profiling.h"
#include "render_utils.h"
#include "render_resources.h"
#include "transform.hlsli"

static dx_pipeline simplePipeline;
static dx_pipeline unlitPositionPipeline;
static dx_pipeline unlitPositionColorPipeline;
static dx_pipeline unlitLinePositionPipeline;
static dx_pipeline unlitLinePositionColorPipeline;




static D3D12_INPUT_ELEMENT_DESC inputLayout_position_color[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};



void debug_simple_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_normal)
		.renderTargets(ldrFormat, depthStencilFormat);

	simplePipeline = createReloadablePipeline(desc, { "flat_simple_vs", "flat_simple_ps" });
}

PIPELINE_SETUP_IMPL(debug_simple_pipeline)
{
	cl->setPipelineState(*simplePipeline.pipeline);
	cl->setGraphicsRootSignature(*simplePipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setGraphicsDynamicConstantBuffer(FLAT_SIMPLE_RS_CAMERA, common.cameraCBV);
}

PIPELINE_RENDER_IMPL(debug_simple_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_SIMPLE_RS_TRANFORM, transform_cb{ viewProj * rc.data.transform, rc.data.transform });
	cl->setGraphics32BitConstants(FLAT_SIMPLE_RS_CB, visualization_cb{ rc.data.color });
	cl->setVertexBuffer(0, rc.data.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.data.vertexBuffer.others);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}





void debug_unlit_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.cullingOff()
		.renderTargets(ldrFormat, depthStencilFormat);
	unlitPositionPipeline = createReloadablePipeline(desc, { "flat_unlit_position_vs", "flat_unlit_ps" });

	desc.inputLayout(inputLayout_position_color);
	unlitPositionColorPipeline = createReloadablePipeline(desc, { "flat_unlit_position_color_vs", "flat_unlit_ps" });
}

PIPELINE_SETUP_IMPL(debug_unlit_pipeline::position)
{
	cl->setPipelineState(*unlitPositionPipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitPositionPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_SETUP_IMPL(debug_unlit_pipeline::position_color)
{
	cl->setPipelineState(*unlitPositionColorPipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitPositionColorPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(debug_unlit_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_TRANFORM, viewProj * rc.data.transform);
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_CB, visualization_cb{ rc.data.color });
	cl->setVertexBuffer(0, rc.data.vertexBuffer.positions);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}




void debug_unlit_line_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(ldrFormat, depthStencilFormat)
		.primitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
	unlitLinePositionPipeline = createReloadablePipeline(desc, { "flat_unlit_position_vs", "flat_unlit_ps" });

	desc.inputLayout(inputLayout_position_color);
	unlitLinePositionColorPipeline = createReloadablePipeline(desc, { "flat_unlit_position_color_vs", "flat_unlit_ps" });
}

PIPELINE_SETUP_IMPL(debug_unlit_line_pipeline::position)
{
	cl->setPipelineState(*unlitLinePositionPipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitLinePositionPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

PIPELINE_SETUP_IMPL(debug_unlit_line_pipeline::position_color)
{
	cl->setPipelineState(*unlitLinePositionColorPipeline.pipeline);
	cl->setGraphicsRootSignature(*unlitLinePositionColorPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

PIPELINE_RENDER_IMPL(debug_unlit_line_pipeline)
{
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_TRANFORM, viewProj * rc.data.transform);
	cl->setGraphics32BitConstants(FLAT_UNLIT_RS_CB, rc.data.color);
	cl->setVertexBuffer(0, rc.data.vertexBuffer.positions);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}




void renderDisk(vec3 position, vec3 upAxis, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	const uint32 numSegments = 32;

	uint32 numTriangles = numSegments;
	uint32 numVertices = numSegments + 1;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numTriangles * 3);

	vec3* vertices = (vec3*)vertexPtr;
	indexed_triangle16* triangles = (indexed_triangle16*)indexPtr;

	float deltaRot = M_TAU / numSegments;
	float rot = 0.f;
	for (uint32 i = 0; i < numSegments; ++i)
	{
		*vertices++ = vec3(cos(rot), sin(rot), 0.f) * radius;
		rot += deltaRot;
	}

	*vertices++ = vec3(0.f, 0.f, 0.f);

	uint16 center = (uint16)numVertices - 1;

	for (uint16 i = 0; i < numSegments; ++i)
	{
		uint16 next = i + 1;
		if (i == numSegments - 1)
		{
			next = 0;
		}
		*triangles++ = { center, i, next };
	}

	renderDebug<debug_unlit_pipeline::position>(createModelMatrix(position, rotateFromTo(vec3(0.f, 0.f, 1.f), upAxis)), vb, ib, color, renderPass, overlay);
}

void renderRing(vec3 position, vec3 upAxis, float outerRadius, float innerRadius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	const uint32 numSegments = 32;

	uint32 numTriangles = numSegments * 2;
	uint32 numVertices = numSegments * 2;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numTriangles * 3);

	vec3* outer = (vec3*)vertexPtr;
	vec3* inner = outer + numSegments;
	indexed_triangle16* triangles = (indexed_triangle16*)indexPtr;

	float deltaRot = M_TAU / numSegments;
	float rot = 0.f;
	for (uint32 i = 0; i < numSegments; ++i)
	{
		vec3 p = vec3(cos(rot), sin(rot), 0.f);
		*outer++ = p * outerRadius;
		*inner++ = p * innerRadius;
		rot += deltaRot;
	}

	uint16 offset = (uint16)numSegments;
	for (uint16 i = 0; i < numSegments; ++i)
	{
		uint16 next = i + 1;
		if (i == numSegments - 1)
		{
			next = 0;
		}
		*triangles++ = { i, next, (uint16)(i + offset) };
		*triangles++ = { next, (uint16)(i + offset), (uint16)(next + offset) };
	}

	renderDebug<debug_unlit_pipeline::position>(createModelMatrix(position, rotateFromTo(vec3(0.f, 0.f, 1.f), upAxis)), vb, ib, color, renderPass, overlay);
}

void renderAngleRing(vec3 position, vec3 upAxis, float outerRadius, float innerRadius, 
	vec3 zeroDegAxis, float minAngle, float maxAngle, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	const uint32 numSegments360 = 32;

	const float deltaRot = M_TAU / numSegments360;
	float angleRange = maxAngle - minAngle;
	const uint32 numSegments = (uint32)ceil(angleRange / deltaRot);

	uint32 numTriangles = numSegments * 2;
	uint32 numVertices = (numSegments + 1) * 2;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numTriangles * 3);

	vec3* outer = (vec3*)vertexPtr;
	vec3* inner = outer + (numSegments + 1);
	indexed_triangle16* triangles = (indexed_triangle16*)indexPtr;

	vec3 x = zeroDegAxis;
	vec3 z = cross(upAxis, x);

	float rot = minAngle;
	for (uint32 i = 0; i < numSegments + 1; ++i)
	{
		vec3 p = cos(rot) * x + sin(rot) * z;
		*outer++ = p * outerRadius + position;
		*inner++ = p * innerRadius + position;
		rot += deltaRot;
		rot = min(rot, maxAngle);
	}

	uint16 offset = (uint16)(numSegments + 1);
	for (uint16 i = 0; i < numSegments; ++i)
	{
		uint16 next = i + 1;
		*triangles++ = { i, next, (uint16)(i + offset) };
		*triangles++ = { next, (uint16)(i + offset), (uint16)(next + offset) };
	}

	renderDebug<debug_unlit_pipeline::position>(mat4::identity, vb, ib, color, renderPass, overlay);
}












static std::tuple<dx_dynamic_vertex_buffer, dx_dynamic_index_buffer> getWireRing()
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

	return { vb, ib };
}

static std::tuple<dx_dynamic_vertex_buffer, dx_dynamic_index_buffer> getWireCapsuleCrossSection(float length, float radius)
{
	const uint32 numSegmentsPerHalf = 16;

	uint32 numVertices = numSegmentsPerHalf * 2 + 2;
	uint32 numLines = numVertices;

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), numLines * 2);

	vec3* verticesA = (vec3*)vertexPtr;
	vec3* verticesB = verticesA + (numSegmentsPerHalf + 1);
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

void renderLine(vec3 positionA, vec3 positionB, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render line");

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), 2);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 2);

	vec3* vertices = (vec3*)vertexPtr;
	indexed_line16* lines = (indexed_line16*)indexPtr;

	*vertices++ = positionA;
	*vertices++ = positionB;

	*lines++ = { 0, 1 };

	renderDebug<debug_unlit_line_pipeline::position>(mat4::identity, vb, ib, color, renderPass, overlay);
}

void renderWireSphere(vec3 position, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire sphere");

	auto [vb, ib] = getWireRing();

	renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(position, quat::identity, radius), vb, ib, color, renderPass, overlay);
	renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(position, quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), radius), vb, ib, color, renderPass, overlay);
	renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(position, quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), radius), vb, ib, color, renderPass, overlay);
}

void renderWireCapsule(vec3 positionA, vec3 positionB, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire capsule");

	vec3 center = 0.5f * (positionA + positionB);
	quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), positionA - positionB);
	auto [vb, ib] = getWireCapsuleCrossSection(length(positionA - positionB), radius);

	renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(center, rotation * quat::identity, 1.f), vb, ib, color, renderPass, overlay);
	renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(center, rotation * quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), 1.f), vb, ib, color, renderPass, overlay);
}

void renderWireCylinder(vec3 positionA, vec3 positionB, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire cylinder");

	quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), positionA - positionB);

	{
		auto [vb, ib] = getWireRing();
		quat rotate90deg(vec3(1.f, 0.f, 0.f), deg2rad(90.f));
		renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(positionA, rotation * rotate90deg, radius), vb, ib, color, renderPass, overlay);
		renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(positionB, rotation * rotate90deg, radius), vb, ib, color, renderPass, overlay);
	}

	{
		auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), 8);
		auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 8);

		vec3* vertices = (vec3*)vertexPtr;
		indexed_line16* lines = (indexed_line16*)indexPtr;

		vec3 xAxis = rotation * vec3(radius, 0.f, 0.f);
		vec3 zAxis = rotation * vec3(0.f, 0.f, radius);

		*vertices++ = positionA + xAxis;
		*vertices++ = positionB + xAxis;

		*vertices++ = positionA + zAxis;
		*vertices++ = positionB + zAxis;

		*vertices++ = positionA - xAxis;
		*vertices++ = positionB - xAxis;

		*vertices++ = positionA - zAxis;
		*vertices++ = positionB - zAxis;

		*lines++ = { 0, 1 };
		*lines++ = { 2, 3 };
		*lines++ = { 4, 5 };
		*lines++ = { 6, 7 };

		renderDebug<debug_unlit_line_pipeline::position>(mat4::identity, vb, ib, color, renderPass, overlay);
	}
}

void renderWireCone(vec3 position, vec3 direction, float distance, float angle, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire cone");

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

	renderDebug<debug_unlit_line_pipeline::position>(mat4::identity, vb, ib, color, renderPass, overlay);
}

void renderWireBox(vec3 position, vec3 radius, quat rotation, vec4 color, ldr_render_pass* renderPass, bool overlay)
{
	CPU_PROFILE_BLOCK("Render wire box");

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

	renderDebug<debug_unlit_line_pipeline::position>(createModelMatrix(position, rotation, radius), vb, ib, color, renderPass, overlay);
}

void renderCameraFrustum(const render_camera& frustum, vec4 color, ldr_render_pass* renderPass, float alternativeFarPlane, bool overlay)
{
	CPU_PROFILE_BLOCK("Render camera frustum");

	auto [vb, vertexPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), 8);
	auto [ib, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), 12 * 2);

	vec3* vertices = (vec3*)vertexPtr;
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

	renderDebug<debug_unlit_line_pipeline::position>(mat4::identity, vb, ib, color, renderPass, overlay);
}
