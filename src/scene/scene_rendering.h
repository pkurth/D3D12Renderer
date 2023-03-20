#pragma once

#include "scene.h"
#include "core/memory.h"
#include "rendering/render_pass.h"


void renderScene(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass,
	float dt);

