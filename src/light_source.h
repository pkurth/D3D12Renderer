#pragma once

#include "math.h"
#include "camera.h"

#include "light_source.hlsli"

#define SHADOW_MAP_NEGATIVE_Z_OFFSET 200.f

struct directional_light
{
	vec3 direction;
	uint32 numShadowCascades;

	vec3 color;
	float intensity; // Final radiance is color * intensity.

	vec4 cascadeDistances;
	vec4 bias;

	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 blendDistances;
	uint32 shadowDimensions;

	bool stabilize;

	void updateMatrices(const render_camera& camera);
};

mat4 getSpotLightViewProjectionMatrix(const spot_light_cb& sl);

