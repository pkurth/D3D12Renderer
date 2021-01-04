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

void global_illumination_render_pass::specularReflection(pbr_raytracing_binding_table& bindingTable, raytracing_tlas& tlas)
{
	this->bindingTable = &bindingTable;
	this->tlas = &tlas;
}

void global_illumination_render_pass::reset()
{
	bindingTable = 0;
	tlas = 0;
}
