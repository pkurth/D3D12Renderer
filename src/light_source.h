#pragma once

#include "math.h"
#include "camera.h"

#include "light_source.hlsli"

#define SUN_SHADOW_DIMENSIONS 2048
#define SUN_SHADOW_TEXEL_SIZE (1.f / SUN_SHADOW_DIMENSIONS)
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 1000.f


struct directional_light
{
	vec3 direction;
	uint32 numShadowCascades;

	vec3 color;
	float intensity; // Final radiance is color * intensity.

	vec4 cascadeDistances;
	vec4 bias;

	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];

	float blendArea;


	void updateMatrices(const render_camera& camera);
};

