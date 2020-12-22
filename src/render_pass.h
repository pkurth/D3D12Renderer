#pragma once

#include "math.h"
#include "bounding_volumes.h"
#include "mesh.h"
#include "light_source.h"

#include <functional>

struct pbr_material;
struct raytracing_blas;
struct dx_vertex_buffer;
struct dx_index_buffer;
struct pbr_raytracing_binding_table;
struct raytracing_tlas;
struct dx_render_target;
struct pbr_render_resources;


struct geometry_render_pass
{
	void renderStaticObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<pbr_material>& material, const mat4& transform,
		bool outline = false);
	void renderDynamicObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<pbr_material>& material, 
		const mat4& transform, const mat4& prevFrameTransform, bool outline = false);
	void renderAnimatedObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_vertex_buffer>& prevFrameVertexBuffer,
		submesh_info submesh, submesh_info prevFrameSubmesh,
		const ref<dx_index_buffer>& indexBuffer, const ref<pbr_material>& material,
		const mat4& transform, const mat4& prevFrameTransform, bool outline = false); // Pass null as prevFrameVertexBuffer, if the object hasn't been rendered last frame.

private:
	void reset();

	enum outlined_object_type : uint16
	{
		outlined_static,
		outlined_dynamic,
		outlined_animated,
	};

	struct outline_reference
	{
		outlined_object_type type;
		uint16 index;
	};
	
	struct static_draw_call
	{
		const mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		ref<pbr_material> material;
		submesh_info submesh;
	};

	struct dynamic_draw_call
	{
		const mat4 transform;
		const mat4 prevFrameTransform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		ref<pbr_material> material;
		submesh_info submesh;
	};

	struct animated_draw_call
	{
		const mat4 transform;
		const mat4 prevFrameTransform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_vertex_buffer> prevFrameVertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		ref<pbr_material> material;
		submesh_info submesh;
		submesh_info prevFrameSubmesh;
	};

	std::vector<static_draw_call> staticDrawCalls;
	std::vector<dynamic_draw_call> dynamicDrawCalls;
	std::vector<animated_draw_call> animatedDrawCalls;

	std::vector<outline_reference> outlinedObjects;

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

struct visualization_render_pass
{
	void renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform, vec4 color);

private:
	void reset();

	struct draw_call
	{
		const mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;

		vec4 color;
	};

	std::vector<draw_call> drawCalls;

	friend struct dx_renderer;
};

struct global_illumination_render_pass
{
	void specularReflection(pbr_raytracing_binding_table& bindingTable, raytracing_tlas& tlas);

private:
	void reset();

	pbr_raytracing_binding_table* bindingTable;
	raytracing_tlas* tlas;

	friend struct dx_renderer;
};

struct application_render_pass
{
	using func_t = std::function<void(const dx_render_target& renderTarget, const pbr_render_resources& pbrResources)>;

	void render(const func_t& func);
	void render(func_t&& func);

private:
	void reset();

	std::vector<func_t> drawCalls;

	friend struct dx_renderer;
};

