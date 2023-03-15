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



	template <typename pipeline_t>
	void renderObject(const typename pipeline_t::render_data_t& renderData)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		pass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t, typename depth_prepass_pipeline_t>
	void renderObject(const typename pipeline_t::render_data_t& renderData, 
		const typename depth_prepass_pipeline_t::render_data_t& depthPrepassRenderData, 
		uint32 objectID = -1)
	{
		renderObject<pipeline_t>(renderData);

		{
			using render_data_t = typename depth_prepass_pipeline_t::render_data_t;

			uint64 sortKey = (uint64)depth_prepass_pipeline_t::setup;
			depthPrepass.emplace_back<depth_prepass_pipeline_t, depth_only_render_command<render_data_t>>(sortKey, objectID, depthPrepassRenderData);
		}
	}

	template <typename pipeline_t>
	void renderObject(typename pipeline_t::render_data_t&& renderData)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		pass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(renderData));
	}

	template <typename pipeline_t, typename depth_prepass_pipeline_t>
	void renderObject(typename pipeline_t::render_data_t&& renderData,
		typename depth_prepass_pipeline_t::render_data_t&& depthPrepassRenderData,
		uint32 objectID = -1)
	{
		renderObject<pipeline_t>(std::move(renderData));

		{
			using render_data_t = typename depth_prepass_pipeline_t::render_data_t;

			uint64 sortKey = (uint64)depth_prepass_pipeline_t::setup;
			depthPrepass.emplace_back<depth_prepass_pipeline_t, depth_only_render_command<render_data_t>>(sortKey, objectID, std::move(depthPrepassRenderData));
		}
	}



	// Specializations for PBR materials, since these are the common ones.

	void renderStaticObject(const mat4& transform,
		const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		submesh_info submesh,
		const ref<pbr_material>& material,
		uint32 objectID = -1)
	{
		pbr_render_data data;
		data.transform = transform;
		data.vertexBuffer = vertexBuffer;
		data.indexBuffer = indexBuffer;
		data.submesh = submesh;
		data.material = material;

		switch (material->shader)
		{
			case pbr_material_shader_default:
			case pbr_material_shader_double_sided:
			{
				static_depth_prepass_data prepassData;
				prepassData.transform = transform;
				prepassData.vertexBuffer = vertexBuffer.positions;
				prepassData.indexBuffer = indexBuffer;
				prepassData.submesh = submesh;

				if (material->shader == pbr_material_shader_default)
				{
					renderObject<pbr_pipeline::opaque, static_depth_prepass_pipeline::single_sided>(data, prepassData, objectID);
				}
				else
				{
					renderObject<pbr_pipeline::opaque_double_sided, static_depth_prepass_pipeline::double_sided>(data, prepassData, objectID);
				}
			} break;
			case pbr_material_shader_alpha_cutout:
			{
				static_alpha_cutout_depth_prepass_data prepassData;
				prepassData.transform = transform;
				prepassData.vertexBuffer = vertexBuffer;
				prepassData.indexBuffer = indexBuffer;
				prepassData.submesh = submesh;
				prepassData.alphaCutoutTextureSRV = material->albedo->defaultSRV;
				
				renderObject<pbr_pipeline::opaque_double_sided, static_depth_prepass_pipeline::alpha_cutout>(data, prepassData, objectID);
			} break;
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
		pbr_render_data data;
		data.transform = transform;
		data.vertexBuffer = vertexBuffer;
		data.indexBuffer = indexBuffer;
		data.submesh = submesh;
		data.material = material;

		switch (material->shader)
		{
			case pbr_material_shader_default:
			case pbr_material_shader_double_sided:
			{
				dynamic_depth_prepass_data prepassData;
				prepassData.transform = transform;
				prepassData.prevFrameTransform = prevFrameTransform;
				prepassData.vertexBuffer = vertexBuffer.positions;
				prepassData.indexBuffer = indexBuffer;
				prepassData.submesh = submesh;

				if (material->shader == pbr_material_shader_default)
				{
					renderObject<pbr_pipeline::opaque, dynamic_depth_prepass_pipeline::single_sided>(data, prepassData, objectID);
				}
				else
				{
					renderObject<pbr_pipeline::opaque_double_sided, dynamic_depth_prepass_pipeline::double_sided>(data, prepassData, objectID);
				}
			} break;
			case pbr_material_shader_alpha_cutout:
			{
				dynamic_alpha_cutout_depth_prepass_data prepassData;
				prepassData.transform = transform;
				prepassData.prevFrameTransform = prevFrameTransform;
				prepassData.vertexBuffer = vertexBuffer;
				prepassData.indexBuffer = indexBuffer;
				prepassData.submesh = submesh;
				prepassData.alphaCutoutTextureSRV = material->albedo->defaultSRV;

				renderObject<pbr_pipeline::opaque_double_sided, dynamic_depth_prepass_pipeline::alpha_cutout>(data, prepassData, objectID);
			} break;
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
		pbr_render_data data;
		data.transform = transform;
		data.vertexBuffer = vertexBuffer;
		data.indexBuffer = indexBuffer;
		data.submesh = submesh;
		data.material = material;

		switch (material->shader)
		{
			case pbr_material_shader_default:
			case pbr_material_shader_double_sided:
			{
				animated_depth_prepass_data prepassData;
				prepassData.transform = transform;
				prepassData.prevFrameTransform = prevFrameTransform;
				prepassData.vertexBuffer = vertexBuffer.positions;
				prepassData.prevFrameVertexBufferAddress = prevFrameVertexBuffer.positions ? prevFrameVertexBuffer.positions.view.BufferLocation : vertexBuffer.positions.view.BufferLocation;
				prepassData.indexBuffer = indexBuffer;
				prepassData.submesh = submesh;

				if (material->shader == pbr_material_shader_default)
				{
					renderObject<pbr_pipeline::opaque, animated_depth_prepass_pipeline::single_sided>(data, prepassData, objectID);
				}
				else
				{
					renderObject<pbr_pipeline::opaque_double_sided, animated_depth_prepass_pipeline::double_sided>(data, prepassData, objectID);
				}
			} break;
			case pbr_material_shader_alpha_cutout:
			{
#if 0
				animated_alpha_cutout_depth_prepass_data prepassData;
				prepassData.transform = transform;
				prepassData.prevFrameTransform = prevFrameTransform;
				prepassData.vertexBuffer = vertexBuffer;
				prepassData.prevFrameVertexBufferAddress = prevFrameVertexBuffer.positions ? prevFrameVertexBuffer.positions.view.BufferLocation : vertexBuffer.positions.view.BufferLocation;
				prepassData.indexBuffer = indexBuffer;
				prepassData.submesh = submesh;
				prepassData.alphaTexture = material->albedo;

				renderObject<pbr_pipeline::opaque_double_sided, animated_depth_prepass_pipeline::alpha_cutout>(data, prepassData, objectID);
#endif
				ASSERT(false);
			} break;
		}
	}

	default_render_command_buffer<uint64> pass;
	depth_prepass_render_command_buffer<uint64> depthPrepass;
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
	void renderObject(const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		pass.emplace_back<pipeline_t, render_command<render_data_t>>(-depth, data); // Negative depth -> sort from back to front.
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
		using render_data_t = typename pbr_pipeline::transparent::render_data_t;

		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<pbr_pipeline::transparent, render_command<render_data_t>>(-depth); // Negative depth -> sort from back to front.
		command.data.transform = transform;
		command.data.vertexBuffer = vertexBuffer;
		command.data.indexBuffer = indexBuffer;
		command.data.submesh = submesh;
		command.data.material = material;
	}

	default_render_command_buffer<float> pass;
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
	void renderObject(const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		ldrPass.emplace_back<pipeline_t, render_command<render_data_t>>(depth, data);
	}

	template <typename pipeline_t>
	void renderObject(typename pipeline_t::render_data_t&& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		ldrPass.emplace_back<pipeline_t, render_command<render_data_t>>(depth, std::move(data));
	}

	template <typename pipeline_t>
	void renderOverlay(const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		overlays.emplace_back<pipeline_t, render_command<render_data_t>>(depth, data);
	}

	template <typename pipeline_t>
	void renderOverlay(typename pipeline_t::render_data_t&& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		float depth = 0.f; // TODO
		overlays.emplace_back<pipeline_t, render_command<render_data_t>>(depth, std::move(data));
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

	default_render_command_buffer<float> ldrPass;
	default_render_command_buffer<float> overlays;
	std::vector<outline_render_command> outlines;
};





struct shadow_render_pass_base
{
	void sort()
	{
		staticPass.sort();
		dynamicPass.sort();
	}

	void reset()
	{
		staticPass.clear();
		dynamicPass.clear();
	}


	template <typename pipeline_t>
	void renderStaticObject(const typename pipeline_t::render_data_t& renderData)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		staticPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t>
	void renderStaticObject(typename pipeline_t::render_data_t&& renderData)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		staticPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(renderData));
	}

	template <typename pipeline_t>
	void renderDynamicObject(const typename pipeline_t::render_data_t& renderData)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		dynamicPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t>
	void renderDynamicObject(typename pipeline_t::render_data_t&& renderData)
	{
		using render_data_t = typename pipeline_t::render_data_t;

		uint64 sortKey = (uint64)pipeline_t::setup;
		dynamicPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(renderData));
	}

	default_render_command_buffer<uint64> staticPass;
	default_render_command_buffer<uint64> dynamicPass;
};

struct sun_cascade_render_pass : shadow_render_pass_base
{
	mat4 viewProj;
	shadow_map_viewport viewport;
};

struct sun_shadow_render_pass
{
	// Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.

	template <typename pipeline_t>
	void renderStaticObject(uint32 cascadeIndex, const typename pipeline_t::render_data_t& renderData)
	{
		cascades[cascadeIndex].renderStaticObject<pipeline_t>(renderData);
	}

	template <typename pipeline_t>
	void renderStaticObject(uint32 cascadeIndex, typename pipeline_t::render_data_t&& renderData)
	{
		cascades[cascadeIndex].renderStaticObject<pipeline_t>(std::move(renderData));
	}

	template <typename pipeline_t>
	void renderDynamicObject(uint32 cascadeIndex, const typename pipeline_t::render_data_t& renderData)
	{
		cascades[cascadeIndex].renderDynamicObject<pipeline_t>(renderData);
	}

	template <typename pipeline_t>
	void renderDynamicObject(uint32 cascadeIndex, typename pipeline_t::render_data_t&& renderData)
	{
		cascades[cascadeIndex].renderDynamicObject<pipeline_t>(std::move(renderData));
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



enum compute_pass_event
{
	compute_pass_frame_start,
	compute_pass_before_depth_prepass,
	compute_pass_before_opaque,
	compute_pass_before_transparent_and_post_processing,

	compute_pass_event_count,
};

struct compute_pass
{
	void reset()
	{
		particleSystemUpdates.clear();

		for (uint32 i = 0; i < compute_pass_event_count; ++i)
		{
			passes[i].clear();
		}
	}

	void updateParticleSystem(struct particle_system* p)
	{
		particleSystemUpdates.push_back(p);
	}

	template <typename pipeline_t>
	void addTask(compute_pass_event eventTime, const typename pipeline_t::render_data_t& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;
		passes[eventTime].emplace_back<pipeline_t, render_command<render_data_t>>(eventTime, data);
	}

	template <typename pipeline_t>
	void addTask(compute_pass_event eventTime, typename pipeline_t::render_data_t&& data)
	{
		using render_data_t = typename pipeline_t::render_data_t;
		passes[eventTime].emplace_back<pipeline_t, render_command<render_data_t>>(eventTime, std::move(data));
	}


	float dt;
	std::vector<struct particle_system*> particleSystemUpdates;

	compute_command_buffer<uint64> passes[compute_pass_event_count];
};


