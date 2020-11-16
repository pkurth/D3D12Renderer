#pragma once

#include "math.h"
#include "camera.h"

#include "light_source.hlsl"

#define SUN_SHADOW_DIMENSIONS 2048
#define SUN_SHADOW_TEXEL_SIZE (1.f / SUN_SHADOW_DIMENSIONS)
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f


struct directional_light
{
	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];
	vec4 cascadeDistances;
	vec4 bias;

	vec3 direction;
	float blendArea;

	vec3 radiance;
	uint32 numShadowCascades;

	void updateMatrices(const render_camera& camera);
};

