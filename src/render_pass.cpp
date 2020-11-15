#include "pch.h"
#include "render_pass.h"
#include "camera.h"
#include "dx_renderer.h"
#include "dx_context.h"

void geometry_render_pass::reset()
{
	drawCalls.clear();
}

void geometry_render_pass::renderObject(const dx_mesh* mesh, submesh_info submesh, const pbr_material* material, const mat4& transform)
{
	drawCalls.push_back(
		{
			transform,
			mesh,
			material,
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

void sun_shadow_render_pass::renderObject(uint32 cascadeIndex, const dx_mesh* mesh, submesh_info submesh, const mat4& transform)
{
	drawCalls[cascadeIndex].push_back(
		{
			transform,
			mesh,
			submesh,
		}
	);
}
