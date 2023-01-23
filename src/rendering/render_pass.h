#pragma once

#include "core/math.h"
#include "core/cpu_profiling.h"
#include "material.h"
#include "render_command.h"
#include "render_command_buffer.h"
#include "pbr.h"
#include "depth_prepass.h"


struct opaque_render_pass
{
	void sort()
	{
		CPU_PROFILE_BLOCK("Sort opaque render passes");

		pass.sort();
		depthPrepass.sort();
	}

	void reset()
	{
		pass.clear();
		depthPrepass.clear();
	}

	void renderStaticObject(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const ref<pbr_material>& material,
		uint32 objectID = -1)
	{
		if (material->doubleSided)
		{
			addPBRObjectToPass<pbr_pipeline::opaque_double_sided>(transform, vertexBuffer, indexBuffer, submesh, material);
			addStaticPBRObjectToDepthPrepass<static_depth_prepass_pipeline::double_sided>(transform, vertexBuffer, indexBuffer, submesh, objectID);
		}
		else
		{
			addPBRObjectToPass<pbr_pipeline::opaque>(transform, vertexBuffer, indexBuffer, submesh, material);
			addStaticPBRObjectToDepthPrepass<static_depth_prepass_pipeline::single_sided>(transform, vertexBuffer, indexBuffer, submesh, objectID);
		}
	}

	void renderDynamicObject(const mat4& transform,
		const mat4& prevFrameTransform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const ref<pbr_material>& material,
		uint32 objectID = -1)
	{
		if (material->doubleSided)
		{
			addPBRObjectToPass<pbr_pipeline::opaque_double_sided>(transform, vertexBuffer, indexBuffer, submesh, material);
			addDynamicPBRObjectToDepthPrepass<dynamic_depth_prepass_pipeline::double_sided>(transform, prevFrameTransform, vertexBuffer, indexBuffer, submesh, objectID);
		}
		else
		{
			addPBRObjectToPass<pbr_pipeline::opaque>(transform, vertexBuffer, indexBuffer, submesh, material);
			addDynamicPBRObjectToDepthPrepass<dynamic_depth_prepass_pipeline::single_sided>(transform, prevFrameTransform, vertexBuffer, indexBuffer, submesh, objectID);
		}
	}

	void renderAnimatedObject(const mat4& transform,
		const mat4& prevFrameTransform,
		dx_vertex_buffer_group_view& vertexBuffer,
		dx_vertex_buffer_group_view& prevFrameVertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const ref<pbr_material>& material,
		uint32 objectID = -1)
	{
		if (material->doubleSided)
		{
			addPBRObjectToPass<pbr_pipeline::opaque_double_sided>(transform, vertexBuffer, indexBuffer, submesh, material);
			addAnimatedPBRObjectToDepthPrepass<animated_depth_prepass_pipeline::double_sided>(transform, prevFrameTransform, vertexBuffer, prevFrameVertexBuffer, indexBuffer, submesh, objectID);
		}
		else
		{
			addPBRObjectToPass<pbr_pipeline::opaque>(transform, vertexBuffer, indexBuffer, submesh, material);
			addAnimatedPBRObjectToDepthPrepass<animated_depth_prepass_pipeline::single_sided>(transform, prevFrameTransform, vertexBuffer, prevFrameVertexBuffer, indexBuffer, submesh, objectID);
		}
	}

	render_command_buffer<uint64> pass;
	render_command_buffer<uint64, depth_prepass_render_func> depthPrepass;

private:

	template <typename pipeline_t>
	void addPBRObjectToPass(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh, 
		const ref<pbr_material>& material)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		auto& command = pass.emplace_back<pipeline_t, default_render_command<render_data_t>>(sortKey);
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.data = material;
	}
	
	template <typename pipeline_t>
	void addStaticPBRObjectToDepthPrepass(const mat4& transform, 
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer, 
		submesh_info submesh, 
		uint32 objectID)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		auto& command = depthPrepass.emplace_back<pipeline_t, depth_only_render_command<static_depth_prepass_data>>(sortKey);
		command.objectID = objectID;
		command.data.transform = transform;
		command.data.vertexBuffer = vertexBuffer.positions;
		command.data.indexBuffer = indexBuffer;
		command.data.submesh = submesh;
	}

	template <typename pipeline_t>
	void addDynamicPBRObjectToDepthPrepass(const mat4& transform,
		const mat4& prevFrameTransform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		uint32 objectID)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		auto& command = depthPrepass.emplace_back<pipeline_t, depth_only_render_command<dynamic_depth_prepass_data>>(sortKey);
		command.objectID = objectID;
		command.data.transform = transform;
		command.data.prevFrameTransform = prevFrameTransform;
		command.data.vertexBuffer = vertexBuffer.positions;
		command.data.indexBuffer = indexBuffer;
		command.data.submesh = submesh;
	}

	template <typename pipeline_t>
	void addAnimatedPBRObjectToDepthPrepass(const mat4& transform,
		const mat4& prevFrameTransform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_vertex_buffer_group_view& prevFrameVertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		uint32 objectID)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		auto& command = depthPrepass.emplace_back<pipeline_t, depth_only_render_command<animated_depth_prepass_data>>(sortKey);
		command.objectID = objectID;
		command.data.transform = transform;
		command.data.prevFrameTransform = prevFrameTransform;
		command.data.vertexBuffer = vertexBuffer.positions;
		command.data.prevFrameVertexBufferAddress = prevFrameVertexBuffer.positions ? prevFrameVertexBuffer.positions.view.BufferLocation : vertexBuffer.positions.view.BufferLocation;
		command.data.indexBuffer = indexBuffer;
		command.data.submesh = submesh;
	}
};

struct transparent_render_pass
{
	void sort()
	{
		CPU_PROFILE_BLOCK("Sort transparent render pass");

		pass.sort();
	}

	void reset()
	{
		pass.clear();
	}

	template <typename pipeline_t>
	void renderObject(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<pipeline_t, default_render_command<render_data_t>>(-depth); // Negative depth -> sort from back to front.
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.data = data;
	}

	template <typename pipeline_t>
	void renderParticles(const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		const particle_draw_info& drawInfo,
		const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<pipeline_t, particle_render_command<render_data_t>>(-depth); // Negative depth -> sort from back to front.
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.drawInfo = drawInfo;
		command.data = data;
	}

	void renderObject(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const ref<pbr_material>& material)
	{
		renderObject<pbr_pipeline::transparent>(transform, vertexBuffer, indexBuffer, submesh, material);
	}

	render_command_buffer<float> pass;
};

struct ldr_render_pass
{
	void sort()
	{
		CPU_PROFILE_BLOCK("Sort LDR render passes");

		ldrPass.sort();
		overlays.sort();
		// We don't sort the outlines.
	}

	void reset()
	{
		ldrPass.clear();
		overlays.clear();
		outlines.clear();
	}

	template <typename pipeline_t>
	void renderObject(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		auto& command = ldrPass.emplace_back<pipeline_t, default_render_command<render_data_t>>(depth);
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.data = data;
	}

	template <typename pipeline_t>
	void renderOverlay(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		auto& command = overlays.emplace_back<pipeline_t, default_render_command<render_data_t>>(depth);
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
		command.data = data;
	}

	void renderOutline(const mat4& transform,
		const dx_vertex_buffer_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh)
	{
		auto& command = outlines.emplace_back();
		command.transform = transform;
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.submesh = submesh;
	}

	void renderOutline(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh)
	{
		renderOutline(transform, vertexBuffer.positions, indexBuffer, submesh);
	}

	render_command_buffer<float> ldrPass;
	render_command_buffer<float> overlays;
	std::vector<outline_render_command> outlines;
};





struct shadow_render_pass_base
{
	void renderStaticObject(const mat4& transform, const dx_vertex_buffer_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		auto& dcs = doubleSided ? doubleSidedStaticDrawCalls : staticDrawCalls;
		dcs.push_back(
			{
				transform,
				vertexBuffer,
				indexBuffer,
				submesh,
			}
		);
	}

	void renderDynamicObject(const mat4& transform, const dx_vertex_buffer_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		auto& dcs = doubleSided ? doubleSidedDynamicDrawCalls : dynamicDrawCalls;
		dcs.push_back(
			{
				transform,
				vertexBuffer,
				indexBuffer,
				submesh,
			}
		);
	}

	void renderStaticObject(const mat4& transform, const dx_vertex_buffer_group_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		renderStaticObject(transform, vertexBuffer.positions, indexBuffer, submesh);
	}

	void renderDynamicObject(const mat4& transform, const dx_vertex_buffer_group_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		renderDynamicObject(transform, vertexBuffer.positions, indexBuffer, submesh);
	}

	void reset()
	{
		staticDrawCalls.clear();
		dynamicDrawCalls.clear();
		doubleSidedStaticDrawCalls.clear();
		doubleSidedDynamicDrawCalls.clear();
	}

	std::vector<shadow_render_command> staticDrawCalls;
	std::vector<shadow_render_command> dynamicDrawCalls;

	std::vector<shadow_render_command> doubleSidedStaticDrawCalls;
	std::vector<shadow_render_command> doubleSidedDynamicDrawCalls;
};

struct sun_cascade_render_pass : shadow_render_pass_base
{
	mat4 viewProj;
	shadow_map_viewport viewport;
};

struct sun_shadow_render_pass
{
	// Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.
	void renderStaticObject(uint32 cascadeIndex, const mat4& transform, const dx_vertex_buffer_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		cascades[cascadeIndex].renderStaticObject(transform, vertexBuffer, indexBuffer, submesh, doubleSided);
	}

	void renderStaticObject(uint32 cascadeIndex, const mat4& transform, const dx_vertex_buffer_group_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		renderStaticObject(cascadeIndex, transform, vertexBuffer.positions, indexBuffer, submesh, doubleSided);
	}

	void renderDynamicObject(uint32 cascadeIndex, const mat4& transform, const dx_vertex_buffer_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		cascades[cascadeIndex].renderDynamicObject(transform, vertexBuffer, indexBuffer, submesh, doubleSided);
	}

	void renderDynamicObject(uint32 cascadeIndex, const mat4& transform, const dx_vertex_buffer_group_view& vertexBuffer, const dx_index_buffer_view& indexBuffer, submesh_info submesh, bool doubleSided = false)
	{
		renderDynamicObject(cascadeIndex, transform, vertexBuffer.positions, indexBuffer, submesh, doubleSided);
	}

	void reset()
	{
		for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
		{
			cascades[i].reset();
		}

		copyFromStaticCache = false;
	}

	sun_cascade_render_pass cascades[MAX_NUM_SUN_SHADOW_CASCADES];
	uint32 numCascades;
	bool copyFromStaticCache;
};

struct spot_shadow_render_pass : shadow_render_pass_base
{
	mat4 viewProjMatrix;
	shadow_map_viewport viewport;
	bool copyFromStaticCache;

	void reset()
	{
		shadow_render_pass_base::reset();
		copyFromStaticCache = false;
	}
};

// TODO: Split this into positive and negative direction for frustum culling.
struct point_shadow_render_pass : shadow_render_pass_base
{
	shadow_map_viewport viewport0;
	shadow_map_viewport viewport1;
	vec3 lightPosition;
	float maxDistance;
	bool copyFromStaticCache0;
	bool copyFromStaticCache1;

	void reset()
	{
		shadow_render_pass_base::reset();

		copyFromStaticCache0 = false;
		copyFromStaticCache1 = false;
	}
};



struct compute_pass
{
	void reset()
	{
		particleSystemUpdates.clear();
	}

	void updateParticleSystem(struct particle_system* p)
	{
		particleSystemUpdates.push_back(p);
	}

	float dt;
	std::vector<struct particle_system*> particleSystemUpdates;
};


