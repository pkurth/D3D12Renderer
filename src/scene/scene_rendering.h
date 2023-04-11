#pragma once

#include "scene.h"
#include "core/memory.h"
#include "rendering/render_pass.h"


struct scene_lighting
{
	ref<dx_buffer> spotLightBuffer;
	ref<dx_buffer> pointLightBuffer;

	ref<dx_buffer> spotLightShadowInfoBuffer;
	ref<dx_buffer> pointLightShadowInfoBuffer;

	spot_shadow_render_pass* spotShadowRenderPasses;
	point_shadow_render_pass* pointShadowRenderPasses;

	uint32 maxNumSpotShadowRenderPasses;
	uint32 maxNumPointShadowRenderPasses;

	uint32 numSpotShadowRenderPasses = 0;
	uint32 numPointShadowRenderPasses = 0;
};

void renderScene(const render_camera& camera, game_scene& scene, memory_arena& arena, entity_handle selectedObjectID, 
	directional_light& sun, scene_lighting& lighting, bool invalidateShadowMapCache,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	compute_pass* computePass, float dt);

