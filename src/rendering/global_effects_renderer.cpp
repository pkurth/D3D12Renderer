#include "pch.h"
#include "global_effects_renderer.h"
#include "render_resources.h"
#include "render_utils.h"
#include "render_algorithms.h"
#include "dx/dx_context.h"
#include "dx/dx_command_list.h"
#include "dx/dx_profiling.h"

const sun_shadow_render_pass* global_effects_renderer::sunShadowRenderPasses[MAX_NUM_SUN_LIGHT_SHADOW_PASSES];
const spot_shadow_render_pass* global_effects_renderer::spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
const point_shadow_render_pass* global_effects_renderer::pointLightShadowRenderPasses[MAX_NUM_POINT_LIGHT_SHADOW_PASSES];
uint32 global_effects_renderer::numSunLightShadowRenderPasses;
uint32 global_effects_renderer::numSpotLightShadowRenderPasses;
uint32 global_effects_renderer::numPointLightShadowRenderPasses;

const light_probe_grid* global_effects_renderer::lightProbeGrid;
raytracing_tlas* global_effects_renderer::lightProbeTlas;
ref<dx_texture> global_effects_renderer::sky;

void global_effects_renderer::initialize()
{
}

void global_effects_renderer::beginFrame()
{
	numSunLightShadowRenderPasses = 0;
	numSpotLightShadowRenderPasses = 0;
	numPointLightShadowRenderPasses = 0;

	lightProbeGrid = 0;
}

void global_effects_renderer::endFrame()
{
	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	{
		PROFILE_ALL(cl, "Shadow maps");

		cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cl->transitionBarrier(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		shadowPasses(cl, sunShadowRenderPasses, numSunLightShadowRenderPasses,
			spotLightShadowRenderPasses, numSpotLightShadowRenderPasses,
			pointLightShadowRenderPasses, numPointLightShadowRenderPasses);

		cl->transitionBarrier(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	if (dxContext.featureSupport.raytracing() && lightProbeGrid && lightProbeTlas)
	{
		PROFILE_ALL(cl, "Update light probes");

		dxContext.renderQueue.waitForOtherQueue(dxContext.computeQueue); // Wait for AS-rebuilds. TODO: This is not the way to go here. We should wait for the specific value returned by executeCommandList.

		lightProbeGrid->update(cl, *lightProbeTlas, sky);
	}

	dxContext.executeCommandList(cl);
}
