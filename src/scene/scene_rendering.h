#pragma once

#include "scene.h"
#include "core/memory.h"
#include "rendering/render_pass.h"


void renderScene(const render_camera& camera, game_scene& scene, memory_arena& arena, entity_handle selectedObjectID, directional_light& sun, bool invalidateShadowMapCache,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	float dt);

