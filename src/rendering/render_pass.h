#pragma once

#include "core/math.h"
#include "physics/bounding_volumes.h"
#include "geometry/mesh.h"
#include "light_source.h"
#include "material.h"
#include "shadow_map_cache.h"
#include "render_command_buffer.h"

struct raytracing_blas;
struct dx_vertex_buffer;
struct dx_index_buffer;
struct raytracing_tlas;


struct particle_draw_info
{
	ref<dx_buffer> particleBuffer;
	ref<dx_buffer> aliveList;
	ref<dx_buffer> commandBuffer;
	uint32 aliveListOffset;
	uint32 commandBufferOffset;
	uint32 rootParameterOffset;
};



template <typename pipeline_t_>
struct default_render_command
{
	using pipeline_t = pipeline_t_;

	mat4 transform;
	vertex_buffer_group vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	submesh_info submesh;

	typename pipeline_t_::material_t material;
};

template <typename pipeline_t_>
struct particle_render_command
{
	using pipeline_t = pipeline_t_;

	vertex_buffer_group vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	particle_draw_info drawInfo;

	typename pipeline_t_::material_t material;
};

struct static_depth_only_render_command
{
	mat4 transform;
	ref<dx_vertex_buffer> vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct dynamic_depth_only_render_command
{
	mat4 transform;
	mat4 prevFrameTransform;
	ref<dx_vertex_buffer> vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct animated_depth_only_render_command
{
	mat4 transform;
	mat4 prevFrameTransform;
	ref<dx_vertex_buffer> vertexBuffer;
	ref<dx_vertex_buffer> prevFrameVertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	submesh_info submesh;
	submesh_info prevFrameSubmesh;
	uint32 objectID;
};

struct outline_render_command
{
	mat4 transform;
	ref<dx_vertex_buffer> vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	submesh_info submesh;
};

struct opaque_render_pass
{
	void sort()
	{
		staticDepthPrepass.sort();
		dynamicDepthPrepass.sort();
		animatedDepthPrepass.sort();
		pass.sort();
	}

	void reset()
	{
		staticDepthPrepass.clear();
		dynamicDepthPrepass.clear();
		animatedDepthPrepass.clear();
		pass.clear();
	}

	template <typename pipeline_t>
	void renderStaticObject(const mat4& transform,
		const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::material_t& material,
		uint32 objectID = -1)
	{
		renderObjectCommon<pipeline_t>(transform, vertexBuffer, indexBuffer, submesh, material);

		float depth = 0.f; // TODO
		auto& depthCommand = staticDepthPrepass.emplace_back(depth);
		depthCommand.transform = transform;
		depthCommand.vertexBuffer = vertexBuffer.positions;
		depthCommand.indexBuffer = indexBuffer;
		depthCommand.submesh = submesh;
		depthCommand.objectID = objectID;
	}

	template <typename pipeline_t>
	void renderDynamicObject(const mat4& transform,
		const mat4& prevFrameTransform,
		const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::material_t& material,
		uint32 objectID = -1)
	{
		renderObjectCommon<pipeline_t>(transform, vertexBuffer, indexBuffer, submesh, material);

		float depth = 0.f; // TODO
		auto& depthCommand = dynamicDepthPrepass.emplace_back(depth);
		depthCommand.transform = transform;
		depthCommand.prevFrameTransform = prevFrameTransform;
		depthCommand.vertexBuffer = vertexBuffer.positions;
		depthCommand.indexBuffer = indexBuffer;
		depthCommand.submesh = submesh;
		depthCommand.objectID = objectID;
	}

	template <typename pipeline_t>
	void renderAnimatedObject(const mat4& transform,
		const mat4& prevFrameTransform,
		const vertex_buffer_group& vertexBuffer,
		const vertex_buffer_group& prevFrameVertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh,
		submesh_info prevFrameSubmesh,
		const typename pipeline_t::material_t& material,
		uint32 objectID = -1)
	{
		renderObjectCommon<pipeline_t>(transform, vertexBuffer, indexBuffer, submesh, material);

		float depth = 0.f; // TODO
		auto& depthCommand = animatedDepthPrepass.emplace_back(depth);
		depthCommand.transform = transform;
		depthCommand.prevFrameTransform = prevFrameTransform;
		depthCommand.vertexBuffer = vertexBuffer.positions;
		depthCommand.prevFrameVertexBuffer = prevFrameVertexBuffer.positions ? prevFrameVertexBuffer.positions : vertexBuffer.positions;
		depthCommand.indexBuffer = indexBuffer;
		depthCommand.submesh = submesh;
		depthCommand.prevFrameSubmesh = prevFrameVertexBuffer.positions ? prevFrameSubmesh : submesh;
		depthCommand.objectID = objectID;
	}

	sort_key_vector<float, static_depth_only_render_command> staticDepthPrepass;
	sort_key_vector<float, dynamic_depth_only_render_command> dynamicDepthPrepass;
	sort_key_vector<float, animated_depth_only_render_command> animatedDepthPrepass;
	render_command_buffer<uint64> pass;

private:
	template <typename pipeline_t>
	void renderObjectCommon(const mat4& transform,
		const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::material_t& material)
	{
		uint64 sortKey = (uint64)pipeline_t::setupCommon;
		auto& command = pass.emplace_back<default_render_command<pipeline_t>>(sortKey);
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.material = material;
	}
};

struct transparent_render_pass
{
	void sort()
	{
		pass.sort();
	}

	void reset()
	{
		pass.clear();
	}

	template <typename pipeline_t>
	void renderObject(const mat4& transform,
		const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::material_t& material)
	{
		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<default_render_command<pipeline_t>>(-depth); // Negative depth -> sort from back to front.
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.material = material;
	}

	template <typename pipeline_t>
	void renderParticles(const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		const particle_draw_info& drawInfo,
		const typename pipeline_t::material_t& material)
	{
		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<particle_render_command<pipeline_t>>(-depth); // Negative depth -> sort from back to front.
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.drawInfo = drawInfo;
		command.material = material;
	}

	render_command_buffer<float> pass;
};

struct overlay_render_pass
{
	void sort()
	{
		pass.sort();
	}

	void reset()
	{
		pass.clear();
	}

	template <typename pipeline_t>
	void renderObject(const mat4& transform,
		const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::material_t& material)
	{
		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<default_render_command<pipeline_t>>(depth);
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.material = material;
	}

	render_command_buffer<float> pass;
};

struct outline_render_pass
{
	void reset()
	{
		pass.clear();
	}

	void renderOutline(const mat4& transform,
		const vertex_buffer_group& vertexBuffer,
		const ref<dx_index_buffer>& indexBuffer,
		submesh_info submesh)
	{
		auto& command = pass.emplace_back();
		command.transform = transform;
		command.vertexBuffer = vertexBuffer.positions;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
	}

	std::vector<outline_render_command> pass;
};








// Base for all shadow map passes.
struct shadow_render_pass
{
	struct draw_call
	{
		mat4 transform;
		ref<dx_vertex_buffer> vertexBuffer;
		ref<dx_index_buffer> indexBuffer;
		submesh_info submesh;
	};
};

struct sun_cascade_render_pass : shadow_render_pass
{
	void renderStaticObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);
	void renderDynamicObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();


	mat4 viewProj;
	shadow_map_viewport viewport;
	std::vector<draw_call> staticDrawCalls;
	std::vector<draw_call> dynamicDrawCalls;
};

struct sun_shadow_render_pass
{
	// Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.
	void renderStaticObject(uint32 cascadeIndex, const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);
	void renderDynamicObject(uint32 cascadeIndex, const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();

	sun_cascade_render_pass cascades[MAX_NUM_SUN_SHADOW_CASCADES];
	uint32 numCascades;
	bool copyFromStaticCache;
};

struct spot_shadow_render_pass : shadow_render_pass
{
	mat4 viewProjMatrix;
	shadow_map_viewport viewport;
	bool copyFromStaticCache;

	void renderStaticObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);
	void renderDynamicObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();

	std::vector<draw_call> staticDrawCalls;
	std::vector<draw_call> dynamicDrawCalls;
};

struct point_shadow_render_pass : shadow_render_pass
{
	shadow_map_viewport viewport0;
	shadow_map_viewport viewport1;
	vec3 lightPosition;
	float maxDistance;
	bool copyFromStaticCache0;
	bool copyFromStaticCache1;

	// TODO: Split this into positive and negative direction for frustum culling.
	void renderStaticObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);
	void renderDynamicObject(const vertex_buffer_group& vertexBuffer, const ref<dx_index_buffer>& indexBuffer, submesh_info submesh, const mat4& transform);

	void reset();

	std::vector<draw_call> staticDrawCalls;
	std::vector<draw_call> dynamicDrawCalls;
};



