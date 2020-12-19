#include "pch.h"
#include "render_pass.h"
#include "camera.h"
#include "dx_renderer.h"
#include "dx_context.h"




std::pair<uint32, mat4*> skinning_pass::skinObject(const ref<dx_vertex_buffer>& vertexBuffer, submesh_info submesh, uint32 numJoints)
{
	uint32 offset = (uint32)skinningMatrices.size();
	skinningMatrices.resize(skinningMatrices.size() + numJoints);

	uint32 id = (uint32)calls.size();

	calls.push_back(
		{
			vertexBuffer,
			submesh,
			offset,
			numJoints,
			totalNumVertices
		}
	);

	totalNumVertices += submesh.numVertices;

	return { id, skinningMatrices.data() + offset };
}

void skinning_pass::reset()
{
	calls.clear();
	skinningMatrices.clear();
	totalNumVertices = 0;
}

void geometry_render_pass::renderStaticObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<pbr_material>& material, 
	const mat4& transform, bool outline)
{
	staticDrawCalls.push_back(
		{
			transform,
			vertexBuffer,
			indexBuffer,
			material,
			submesh,
		}
	);

	if (outline)
	{
		outlinedObjects.push_back(
			{
				outlined_static, (uint16)(staticDrawCalls.size() - 1)
			}
		);
	}
}

void geometry_render_pass::renderDynamicObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<pbr_material>& material, 
	const mat4& transform, const mat4& prevFrameTransform, bool outline)
{
	dynamicDrawCalls.push_back(
		{
			transform,
			prevFrameTransform,
			vertexBuffer,
			indexBuffer,
			material,
			submesh,
		}
	);

	if (outline)
	{
		outlinedObjects.push_back(
			{
				outlined_dynamic, (uint16)(dynamicDrawCalls.size() - 1)
			}
		);
	}
}

void geometry_render_pass::renderAnimatedObject(uint32 skinID, const ref<dx_index_buffer>& indexBuffer, const ref<pbr_material>& material, const mat4& transform, const mat4& prevFrameTransform, bool outline)
{
	animatedDrawCalls.push_back(
		{
			transform,
			prevFrameTransform,
			skinID,
			indexBuffer,
			material,
		}
	);

	if (outline)
	{
		outlinedObjects.push_back(
			{
				outlined_animated, (uint16)(animatedDrawCalls.size() - 1)
			}
		);
	}
}

void geometry_render_pass::reset()
{
	staticDrawCalls.clear();
	dynamicDrawCalls.clear();
	animatedDrawCalls.clear();
	outlinedObjects.clear();
}

void sun_shadow_render_pass::renderObject(uint32 cascadeIndex, const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	drawCalls[cascadeIndex].push_back(
		{
			transform,
			vertexBuffer,
			indexBuffer,
			submesh,
			shadow_object_default
		}
	);
}

void sun_shadow_render_pass::renderObject(uint32 cascadeIndex, uint32 skinID, const ref<dx_index_buffer>& indexBuffer, const mat4& transform)
{
	drawCalls[cascadeIndex].push_back(
		{
			transform,
			0,
			indexBuffer,
			{},
			shadow_object_animated,
			skinID
		}
	);
}

void sun_shadow_render_pass::reset()
{
	for (uint32 i = 0; i < arraysize(drawCalls); ++i)
	{
		drawCalls[i].clear();
	}
}

void visualization_render_pass::renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform, vec4 color)
{
	drawCalls.push_back(
		{
			transform,
			vertexBuffer,
			indexBuffer,
			submesh,
			color
		}
	);
}

void visualization_render_pass::reset()
{
	drawCalls.clear();
}

void raytraced_reflections_render_pass::renderObject(pbr_raytracing_binding_table& bindingTable, raytracing_tlas& tlas)
{
	drawCalls.push_back({ &bindingTable, &tlas });
}

void raytraced_reflections_render_pass::reset()
{
	drawCalls.clear();
}
