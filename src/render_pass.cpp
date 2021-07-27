#include "pch.h"
#include "render_pass.h"
#include "camera.h"
#include "dx_context.h"



void geometry_render_pass::reset()
{
	drawCalls.clear();
	outlinedObjects.clear();
}

void opaque_render_pass::reset()
{
	geometry_render_pass::reset();

	staticDepthOnlyDrawCalls.clear();
	dynamicDepthOnlyDrawCalls.clear();
	animatedDepthOnlyDrawCalls.clear();
}

void transparent_render_pass::reset()
{
	geometry_render_pass::reset();

	particleDrawCalls.clear();
}

void sun_shadow_render_pass::renderStaticObject(uint32 cascadeIndex, const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	staticDrawCalls[cascadeIndex].push_back(
		{
			transform,
			vertexBuffer.positions,
			indexBuffer,
			submesh,
		}
	);
}

void sun_shadow_render_pass::renderDynamicObject(uint32 cascadeIndex, const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	dynamicDrawCalls[cascadeIndex].push_back(
		{
			transform,
			vertexBuffer.positions,
			indexBuffer,
			submesh,
		}
	);
}

void sun_shadow_render_pass::reset()
{
	for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
	{
		staticDrawCalls[i].clear();
		dynamicDrawCalls[i].clear();
	}

	copyFromStaticCache = false;
}

void spot_shadow_render_pass::renderStaticObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	staticDrawCalls.push_back(
		{
			transform,
			vertexBuffer.positions,
			indexBuffer,
			submesh,
		}
	);
}

void spot_shadow_render_pass::renderDynamicObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	dynamicDrawCalls.push_back(
		{
			transform,
			vertexBuffer.positions,
			indexBuffer,
			submesh,
		}
	);
}

void spot_shadow_render_pass::reset()
{
	staticDrawCalls.clear();
	dynamicDrawCalls.clear();

	copyFromStaticCache = false;
}

void point_shadow_render_pass::renderStaticObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	staticDrawCalls.push_back(
		{
			transform,
			vertexBuffer.positions,
			indexBuffer,
			submesh,
		}
	);
}

void point_shadow_render_pass::renderDynamicObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	dynamicDrawCalls.push_back(
		{
			transform,
			vertexBuffer.positions,
			indexBuffer,
			submesh,
		}
	);
}

void point_shadow_render_pass::reset()
{
	staticDrawCalls.clear();
	dynamicDrawCalls.clear();

	copyFromStaticCache0 = false;
	copyFromStaticCache1 = false;
}
