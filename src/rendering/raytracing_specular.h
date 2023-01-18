#pragma once

#include "dx/dx_texture.h"

void initializeRTReflectionsPipelines();
void raytraceRTReflections(struct dx_command_list* cl, const struct raytracing_tlas& tlas, 
	ref<dx_texture> depthStencilBuffer,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> worldNormalsRoughnessTexture,	// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> raycastTexture,					// UNORDERED_ACCESS
	ref<dx_texture> resolveTexture,					// UNORDERED_ACCESS. After call D3D12_RESOURCE_STATE_GENERIC_READ. Also output of this algorithm.
	ref<dx_texture> temporalHistory,				// NON_PIXEL_SHADER_RESOURCE. After call UNORDERED_ACCESS.
	ref<dx_texture> temporalOutput,					// UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	const struct common_render_data& common);


