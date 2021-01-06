#include "pch.h"
#include "render_pass.h"
#include "camera.h"
#include "dx_renderer.h"
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

void sun_shadow_render_pass::renderObject(uint32 cascadeIndex, const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	drawCalls[cascadeIndex].push_back(
		{
			transform,
			vertexBuffer,
			indexBuffer,
			submesh,
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

void spot_shadow_render_pass::renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	drawCalls.push_back(
		{
			transform,
			vertexBuffer,
			indexBuffer,
			submesh,
		}
	);
}

void spot_shadow_render_pass::reset()
{
	drawCalls.clear();
}

void point_shadow_render_pass::renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform)
{
	drawCalls.push_back(
		{
			transform,
			vertexBuffer,
			indexBuffer,
			submesh,
		}
	);
}

void point_shadow_render_pass::reset()
{
	drawCalls.clear();
}
