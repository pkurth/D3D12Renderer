#include "pch.h"
#include "render_pass.h"
#include "camera.h"
#include "dx_renderer.h"
#include "dx_context.h"




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

void geometry_render_pass::renderAnimatedObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_vertex_buffer>& prevFrameVertexBuffer,
	submesh_info submesh, submesh_info prevFrameSubmesh,
	const ref<dx_index_buffer>& indexBuffer, const ref<pbr_material>& material,
	const mat4& transform, const mat4& prevFrameTransform, bool outline)
{
	animatedDrawCalls.push_back(
		{
			transform,
			prevFrameTransform,
			vertexBuffer,
			prevFrameVertexBuffer,
			indexBuffer,
			material,
			submesh,
			prevFrameSubmesh,
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

void application_render_pass::render(const func_t& func)
{
	drawCalls.push_back(func);
}

void application_render_pass::render(func_t&& func)
{
	drawCalls.emplace_back(std::move(func));
}

void application_render_pass::reset()
{
	drawCalls.clear();
}
