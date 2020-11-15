#pragma once

#include "math.h"
#include "dx_render_primitives.h"
#include "light_source.h"

struct dx_mesh;
struct pbr_material;
struct pbr_environment;
struct render_camera;

struct geometry_render_pass
{
	void renderObject(const dx_mesh* mesh, submesh_info submesh, const pbr_material* material, const mat4& transform);

private:
	void reset();
	
	struct draw_call
	{
		const mat4 transform;
		const dx_mesh* mesh;
		const pbr_material* material;
		submesh_info submesh;
	};

	std::vector<draw_call> drawCalls;

	friend struct dx_renderer;
};

struct sun_shadow_render_pass
{
	// Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.
	void renderObject(uint32 cascadeIndex, const dx_mesh* mesh, submesh_info submesh, const mat4& transform);

private:
	void reset();

	struct draw_call
	{
		const mat4 transform;
		const dx_mesh* mesh;
		submesh_info submesh;
	};

	std::vector<draw_call> drawCalls[MAX_NUM_SUN_SHADOW_CASCADES];

	friend struct dx_renderer;
};

