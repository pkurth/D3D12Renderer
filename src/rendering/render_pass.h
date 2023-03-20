#pragma once

#include "core/math.h"
#include "core/cpu_profiling.h"
#include "material.h"
#include "render_command.h"
#include "render_command_buffer.h"
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



	template <typename pipeline_t, typename render_data_t>
	void renderObject(const render_data_t& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		pass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
		void renderObject(render_data_t&& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		pass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(renderData));
	}


	template <typename pipeline_t, typename render_data_t>
	void renderDepthOnly(const render_data_t& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		depthPrepass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
		void renderDepthOnly(render_data_t&& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		depthPrepass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(renderData));
	}


	template <typename pipeline_t, typename depth_prepass_pipeline_t, typename render_data_t, typename depth_prepass_render_data_t>
	void renderObject(const render_data_t& renderData, 
		const depth_prepass_render_data_t& depthPrepassRenderData)
	{
		renderObject<pipeline_t, render_data_t>(renderData);
		renderDepthOnly<depth_prepass_pipeline_t, depth_prepass_render_data_t>(depthPrepassRenderData);
	}

	template <typename pipeline_t, typename depth_prepass_pipeline_t, typename render_data_t, typename depth_prepass_render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t> && !std::is_lvalue_reference_v<depth_prepass_render_data_t>>>
	void renderObject(render_data_t&& renderData,
		depth_prepass_render_data_t&& depthPrepassRenderData)
	{
		renderObject<pipeline_t, render_data_t>(std::move(renderData));
		renderDepthOnly<depth_prepass_pipeline_t, depth_prepass_render_data_t>(std::move(depthPrepassRenderData));
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

	template <typename pipeline_t, typename render_data_t>
	void renderObject(const render_data_t& data)
	{
		float depth = 0.f; // TODO
		pass.emplace_back<pipeline_t, render_command<render_data_t>>(-depth, data); // Negative depth -> sort from back to front.
	}

	template <typename pipeline_t, typename render_data_t>
	void renderParticles(const dx_vertex_buffer_group_view& vertexBuffer,
		const dx_index_buffer_view& indexBuffer,
		const particle_draw_info& drawInfo,
		const render_data_t& data)
	{
		float depth = 0.f; // TODO
		auto& command = pass.emplace_back<pipeline_t, particle_render_command<render_data_t>>(-depth); // Negative depth -> sort from back to front.
		command.vertexBuffer = vertexBuffer;
		command.indexBuffer = indexBuffer;
		command.drawInfo = drawInfo;
		command.data = data;
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
		outlines.sort();
	}

	void reset()
	{
		ldrPass.clear();
		overlays.clear();
		outlines.clear();
	}

	template <typename pipeline_t, typename render_data_t>
	void renderObject(const render_data_t& data)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		ldrPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, data);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderObject(render_data_t&& data)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		ldrPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(data));
	}

	template <typename pipeline_t, typename render_data_t>
	void renderOverlay(const render_data_t& data)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		overlays.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, data);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderOverlay(render_data_t&& data)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		overlays.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(data));
	}

	template <typename pipeline_t, typename render_data_t>
	void renderOutline(const render_data_t& data)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		outlines.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, data);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderOutline(render_data_t&& data)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		outlines.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(data));
	}

	default_render_command_buffer<uint64> ldrPass;
	default_render_command_buffer<uint64> overlays;
	default_render_command_buffer<uint64> outlines;
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


	template <typename pipeline_t, typename render_data_t>
	void renderStaticObject(const render_data_t& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		staticPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderStaticObject(render_data_t&& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		staticPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, std::move(renderData));
	}

	template <typename pipeline_t, typename render_data_t>
	void renderDynamicObject(const render_data_t& renderData)
	{
		uint64 sortKey = (uint64)pipeline_t::setup;
		dynamicPass.emplace_back<pipeline_t, render_command<render_data_t>>(sortKey, renderData);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderDynamicObject(render_data_t&& renderData)
	{
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

	template <typename pipeline_t, typename render_data_t>
	void renderStaticObject(uint32 cascadeIndex, const render_data_t& renderData)
	{
		cascades[cascadeIndex].renderStaticObject<pipeline_t>(renderData);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderStaticObject(uint32 cascadeIndex, render_data_t&& renderData)
	{
		cascades[cascadeIndex].renderStaticObject<pipeline_t>(std::move(renderData));
	}

	template <typename pipeline_t, typename render_data_t>
	void renderDynamicObject(uint32 cascadeIndex, const render_data_t& renderData)
	{
		cascades[cascadeIndex].renderDynamicObject<pipeline_t>(renderData);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void renderDynamicObject(uint32 cascadeIndex, render_data_t&& renderData)
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

	template <typename pipeline_t, typename render_data_t>
	void addTask(compute_pass_event eventTime, const render_data_t& data)
	{
		passes[eventTime].emplace_back<pipeline_t, render_command<render_data_t>>(eventTime, data);
	}

	template <typename pipeline_t, typename render_data_t,
		class = typename std::enable_if_t<!std::is_lvalue_reference_v<render_data_t>>>
	void addTask(compute_pass_event eventTime, render_data_t&& data)
	{
		passes[eventTime].emplace_back<pipeline_t, render_command<render_data_t>>(eventTime, std::move(data));
	}


	float dt;
	std::vector<struct particle_system*> particleSystemUpdates;

	compute_command_buffer<uint64> passes[compute_pass_event_count];
};


