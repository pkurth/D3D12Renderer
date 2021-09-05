#pragma once

#include "core/math.h"
#include "core/camera.h"

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

	vec4 shadowMapViewports[MAX_NUM_SUN_SHADOW_CASCADES];
	mat4 viewProjs[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 blendDistances;
	uint32 shadowDimensions;

	bool stabilize;

	void updateMatrices(const render_camera& camera);
};

// Simple wrappers around CBs. Don't add additional data here!
struct point_light_component : point_light_cb
{
	point_light_component(vec3 position, vec3 radiance, float radius, int shadowInfoIndex = -1)
		: point_light_cb(position, radiance, radius, shadowInfoIndex) {}
};

struct spot_light_component : spot_light_cb
{
	spot_light_component(vec3 position, vec3 direction, vec3 radiance, float innerAngle, float outerAngle, float maxDistance, int shadowInfoIndex = -1)
		: spot_light_cb(position, direction, radiance, innerAngle, outerAngle, maxDistance, shadowInfoIndex) {}
};

mat4 getSpotLightViewProjectionMatrix(const spot_light_cb& sl);

