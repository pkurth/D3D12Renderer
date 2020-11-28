#pragma once

#include "math.h"
#include "bounding_volumes.h"
#include "dx_render_primitives.h"
#include "light_source.h"

struct pbr_material;
struct raytracing_blas;
struct dx_vertex_buffer;
struct dx_index_buffer;
struct raytracing_batch;

struct geometry_render_pass
{
	void renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<pbr_material>& material, const mat4& transform);

private:
	void reset();
	
	struct draw_call
	{
		const mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		ref<pbr_material> material;
		submesh_info submesh;
	};

	std::vector<draw_call> drawCalls;

	friend struct dx_renderer;
};

struct sun_shadow_render_pass
{
	// Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.
	void renderObject(uint32 cascadeIndex, const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

private:
	void reset();

	struct draw_call
	{
		const mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
	};

	std::vector<draw_call> drawCalls[MAX_NUM_SUN_SHADOW_CASCADES];

	friend struct dx_renderer;
};

struct raytraced_reflections_render_pass
{
	void renderObject(raytracing_batch* batch);

private:
	void reset();

	struct draw_call
	{
		raytracing_batch* batch;
	};

	std::vector<draw_call> drawCalls;

	friend struct dx_renderer;
};

