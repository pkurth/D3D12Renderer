#include "pch.h"
#include "render_pass.h"
#include "core/camera.h"
#include "dx/dx_context.h"


void sun_cascade_render_pass::renderStaticObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
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

void sun_cascade_render_pass::renderDynamicObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
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

void sun_cascade_render_pass::reset()
{
	staticDrawCalls.clear();
	dynamicDrawCalls.clear();
}

void sun_shadow_render_pass::renderStaticObject(uint32 cascadeIndex, const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	cascades[cascadeIndex].renderStaticObject(vertexBuffer, indexBuffer, submesh, transform);
}

void sun_shadow_render_pass::renderDynamicObject(uint32 cascadeIndex, const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	cascades[cascadeIndex].renderDynamicObject(vertexBuffer, indexBuffer, submesh, transform);
}

void sun_shadow_render_pass::reset()
{
	for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
	{
		cascades[i].reset();
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
