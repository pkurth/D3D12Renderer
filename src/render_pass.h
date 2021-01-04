#pragma once

#include "math.h"
#include "bounding_volumes.h"
#include "mesh.h"
#include "light_source.h"
#include "material.h"

struct raytracing_blas;
struct dx_vertex_buffer;
struct dx_index_buffer;
struct pbr_raytracing_binding_table;
struct raytracing_tlas;


// Base for both opaque and transparent pass.
struct geometry_render_pass
{
protected:
	template <typename material_t>
	void common(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<material_t>& material, const mat4& transform,
		bool outline)
	{
		static_assert(std::is_base_of<material_base, material_t>::value, "Material must inherit from material_base.");

		material_setup_function setupFunc = material_t::setupPipeline;

		drawCalls.push_back(
			{
				transform,
				vertexBuffer,
				indexBuffer,
				material,
				submesh,
				setupFunc,
			}
		);

		if (outline)
		{
			outlinedObjects.push_back(
				{
					(uint16)(drawCalls.size() - 1)
				}
			);
		}
	}

	void reset();

private:
	struct draw_call
	{
		const mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		ref<material_base> material;
		submesh_info submesh;
		material_setup_function materialSetupFunc;
	};

	std::vector<draw_call> drawCalls;
	std::vector<uint16> outlinedObjects;

	friend struct dx_renderer;
};

// Renders opaque objects. It also generates screen space velocities, which is why there are three methods for static, dynamic and animated objects.
struct opaque_render_pass : geometry_render_pass
{
	template <typename material_t>
	void renderStaticObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<material_t>& material, const mat4& transform,
		uint16 objectID, bool outline = false)
	{
		common(vertexBuffer, indexBuffer, submesh, material, transform, outline);

		staticDepthOnlyDrawCalls.push_back(
			{
				transform, vertexBuffer, indexBuffer, submesh, objectID
			}
		);
	}

	template <typename material_t>
	void renderDynamicObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<material_t>& material, 
		const mat4& transform, const mat4& prevFrameTransform,
		uint16 objectID, bool outline = false)
	{
		common(vertexBuffer, indexBuffer, submesh, material, transform, outline);

		dynamicDepthOnlyDrawCalls.push_back(
			{
				transform, prevFrameTransform, vertexBuffer, indexBuffer, submesh, objectID
			}
		);
	}

	template <typename material_t>
	void renderAnimatedObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_vertex_buffer>& prevFrameVertexBuffer, 
		const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, submesh_info prevFrameSubmesh, const ref<material_t>& material,
		const mat4& transform, const mat4& prevFrameTransform,
		uint16 objectID, bool outline = false)
	{
		common(vertexBuffer, indexBuffer, submesh, material, transform, outline);

		animatedDepthOnlyDrawCalls.push_back(
			{
				transform, prevFrameTransform, vertexBuffer, 
				prevFrameVertexBuffer ? prevFrameVertexBuffer : vertexBuffer, 
				indexBuffer, 
				submesh, 
				prevFrameVertexBuffer ? prevFrameSubmesh : submesh, 
				objectID
			}
		);
	}

private:
	void reset();

	struct static_depth_only_draw_call
	{
		const mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
		uint16 objectID;
	};

	struct dynamic_depth_only_draw_call
	{
		const mat4 transform;
		const mat4 prevFrameTransform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
		uint16 objectID;
	};

	struct animated_depth_only_draw_call
	{
		const mat4 transform;
		const mat4 prevFrameTransform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_vertex_buffer> prevFrameVertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
		submesh_info prevFrameSubmesh;
		uint16 objectID;
	};

	std::vector<static_depth_only_draw_call> staticDepthOnlyDrawCalls;
	std::vector<dynamic_depth_only_draw_call> dynamicDepthOnlyDrawCalls;
	std::vector<animated_depth_only_draw_call> animatedDepthOnlyDrawCalls;

	friend struct dx_renderer;
};

// Transparent pass currently generates no screen velocities and no object ids.
struct transparent_render_pass : geometry_render_pass
{
	template <typename material_t>
	void renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<material_t>& material, const mat4& transform,
		bool outline = false)
	{
		common(vertexBuffer, indexBuffer, submesh, material, transform, outline);
	}
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

