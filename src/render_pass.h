#pragma once

#include "math.h"
#include "bounding_volumes.h"
#include "mesh.h"
#include "light_source.h"
#include "material.h"

struct raytracing_blas;
struct dx_vertex_buffer;
struct dx_index_buffer;
struct raytracing_tlas;

// Base for both opaque and transparent pass.
struct geometry_render_pass
{
	void reset();

protected:
	template <typename material_t>
	void common(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<material_t>& material, const mat4& transform,
		bool outline, bool setTransform = true)
	{
		static_assert(std::is_base_of<material_base, material_t>::value, "Material must inherit from material_base.");

		material_setup_function setupFunc = material_t::setupPipeline;

		auto& dc = drawCalls.emplace_back();
		dc.transform = transform;
		dc.vertexBuffer = vertexBuffer;
		dc.indexBuffer = indexBuffer;
		dc.material = material;
		dc.submesh = submesh;
		dc.materialSetupFunc = setupFunc;
		dc.drawType = draw_type_default;
		dc.setTransform = setTransform;

		if (outline)
		{
			outlinedObjects.push_back(
				{
					(uint16)(drawCalls.size() - 1)
				}
			);
		}
	}

	template <typename material_t>
	void common(uint32 dispatchX, uint32 dispatchY, uint32 dispatchZ, const ref<material_t>& material, const mat4& transform,
		bool outline, bool setTransform = true)
	{
		static_assert(std::is_base_of<material_base, material_t>::value, "Material must inherit from material_base.");

		material_setup_function setupFunc = material_t::setupPipeline;

		auto& dc = drawCalls.emplace_back();
		dc.transform = transform;
		dc.material = material;
		dc.dispatchInfo = { dispatchX, dispatchY, dispatchZ };
		dc.materialSetupFunc = setupFunc;
		dc.drawType = draw_type_mesh_shader;
		dc.setTransform = setTransform;

		if (outline)
		{
			outlinedObjects.push_back(
				{
					(uint16)(drawCalls.size() - 1)
				}
			);
		}
	}

private:
	enum draw_type
	{
		draw_type_default,
		draw_type_mesh_shader,
	};

	struct dispatch_info 
	{ 
		uint32 dispatchX, dispatchY, dispatchZ; 
	};

	struct draw_call
	{
		mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		ref<material_base> material;
		union
		{
			submesh_info submesh;
			dispatch_info dispatchInfo;
		};
		material_setup_function materialSetupFunc;
		draw_type drawType;
		bool setTransform;
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

	void reset();

private:
	struct static_depth_only_draw_call
	{
		mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
		uint16 objectID;
	};

	struct dynamic_depth_only_draw_call
	{
		mat4 transform;
		mat4 prevFrameTransform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
		uint16 objectID;
	};

	struct animated_depth_only_draw_call
	{
		mat4 transform;
		mat4 prevFrameTransform;
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

	friend struct dx_renderer;
};

struct overlay_render_pass : geometry_render_pass
{
	template <typename material_t>
	void renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const ref<material_t>& material, const mat4& transform, bool setTransform)
	{
		common(vertexBuffer, indexBuffer, submesh, material, transform, false, setTransform);
	}

	template <typename material_t>
	void renderObjectWithMeshShader(uint32 dispatchX, uint32 dispatchY, uint32 dispatchZ, const ref<material_t>& material, const mat4& transform, bool setTransform)
	{
		common(dispatchX, dispatchY, dispatchZ, material, transform, false, setTransform);
	}

	friend struct dx_renderer;
};


// Base for all shadow map passes.
struct shadow_render_pass
{
protected:
	struct draw_call
	{
		mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
	};

	friend struct dx_renderer;
};

struct sun_shadow_render_pass : shadow_render_pass
{
	vec4 viewports[MAX_NUM_SUN_SHADOW_CASCADES];

	// Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.
	void renderObject(uint32 cascadeIndex, const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();

private:
	std::vector<draw_call> drawCalls[MAX_NUM_SUN_SHADOW_CASCADES];

	friend struct dx_renderer;
};

struct spot_shadow_render_pass : shadow_render_pass
{
	mat4 viewProjMatrix;
	vec4 viewport;

	void renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();

private:
	std::vector<draw_call> drawCalls;

	friend struct dx_renderer;
};

struct point_shadow_render_pass : shadow_render_pass
{
	vec4 viewport0;
	vec4 viewport1;
	vec3 lightPosition;
	float maxDistance;

	// TODO: Split this into positive and negative direction for frustum culling.
	void renderObject(const ref<dx_vertex_buffer>& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();

private:
	std::vector<draw_call> drawCalls;

	friend struct dx_renderer;
};



