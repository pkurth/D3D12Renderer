#pragma once

#include "render_pass.h"
#include "light_probe.h"
#include "raytracing_tlas.h"

#define MAX_NUM_SUN_LIGHT_SHADOW_PASSES 16
#define MAX_NUM_SPOT_LIGHT_SHADOW_PASSES 16
#define MAX_NUM_POINT_LIGHT_SHADOW_PASSES 16


struct global_effects_renderer
{
	static void initialize();

	static void beginFrame();
	static void endFrame();

	static void submitShadowRenderPass(sun_shadow_render_pass* renderPass) { assert(numSunLightShadowRenderPasses < MAX_NUM_SUN_LIGHT_SHADOW_PASSES); sunShadowRenderPasses[numSunLightShadowRenderPasses++] = renderPass; }
	static void submitShadowRenderPass(spot_shadow_render_pass* renderPass) { assert(numSpotLightShadowRenderPasses < MAX_NUM_SPOT_LIGHT_SHADOW_PASSES);	spotLightShadowRenderPasses[numSpotLightShadowRenderPasses++] = renderPass; }
	static void submitShadowRenderPass(point_shadow_render_pass* renderPass) { assert(numPointLightShadowRenderPasses < MAX_NUM_POINT_LIGHT_SHADOW_PASSES); pointLightShadowRenderPasses[numPointLightShadowRenderPasses++] = renderPass; }

	static void raytraceLightProbes(const light_probe_grid& grid, raytracing_tlas* tlas, const ref<pbr_environment>& environment) { lightProbeGrid = &grid; lightProbeTlas = tlas; sky = environment->sky; }

private:
	static const sun_shadow_render_pass* sunShadowRenderPasses[MAX_NUM_SUN_LIGHT_SHADOW_PASSES];
	static const spot_shadow_render_pass* spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	static const point_shadow_render_pass* pointLightShadowRenderPasses[MAX_NUM_POINT_LIGHT_SHADOW_PASSES];
	static uint32 numSunLightShadowRenderPasses;
	static uint32 numSpotLightShadowRenderPasses;
	static uint32 numPointLightShadowRenderPasses;

	static const light_probe_grid* lightProbeGrid;
	static raytracing_tlas* lightProbeTlas;
	static ref<dx_texture> sky;

	static light_probe_tracer lightProbeTracer;
};
