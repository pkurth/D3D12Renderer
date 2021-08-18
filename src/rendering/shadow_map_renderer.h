#pragma once

#include "render_pass.h"

#define MAX_NUM_SUN_LIGHT_SHADOW_PASSES 16
#define MAX_NUM_SPOT_LIGHT_SHADOW_PASSES 16
#define MAX_NUM_POINT_LIGHT_SHADOW_PASSES 16


struct shadow_map_renderer
{
	static void beginFrame();
	static void endFrame();

	static void submitRenderPass(sun_shadow_render_pass* renderPass) { assert(numSunLightShadowRenderPasses < MAX_NUM_SUN_LIGHT_SHADOW_PASSES); sunShadowRenderPasses[numSunLightShadowRenderPasses++] = renderPass; }
	static void submitRenderPass(spot_shadow_render_pass* renderPass) { assert(numSpotLightShadowRenderPasses < MAX_NUM_SPOT_LIGHT_SHADOW_PASSES);	spotLightShadowRenderPasses[numSpotLightShadowRenderPasses++] = renderPass; }
	static void submitRenderPass(point_shadow_render_pass* renderPass) { assert(numPointLightShadowRenderPasses < MAX_NUM_POINT_LIGHT_SHADOW_PASSES); pointLightShadowRenderPasses[numPointLightShadowRenderPasses++] = renderPass; }

private:
	static const sun_shadow_render_pass* sunShadowRenderPasses[MAX_NUM_SUN_LIGHT_SHADOW_PASSES];
	static const spot_shadow_render_pass* spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	static const point_shadow_render_pass* pointLightShadowRenderPasses[MAX_NUM_POINT_LIGHT_SHADOW_PASSES];
	static uint32 numSunLightShadowRenderPasses;
	static uint32 numSpotLightShadowRenderPasses;
	static uint32 numPointLightShadowRenderPasses;
};
