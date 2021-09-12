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

struct point_light_component
{
	vec3 color;
	float intensity; // Final radiance is color * intensity.
	float radius;
	bool castsShadow;

	point_light_component() {}
	point_light_component(vec3 color, float intensity, float radius, bool castsShadow = false)
		: color(color), intensity(intensity), radius(radius), castsShadow(castsShadow) {}
};

struct spot_light_component
{
	vec3 color;
	float intensity; // Final radiance is color * intensity.
	float distance;
	float innerAngle;
	float outerAngle;
	bool castsShadow;

	spot_light_component() {}
	spot_light_component(vec3 color, float intensity, float distance, float innerAngle, float outerAngle, bool castsShadow = false)
		: color(color), intensity(intensity), distance(distance), innerAngle(innerAngle), outerAngle(outerAngle), castsShadow(castsShadow) {}
};

mat4 getSpotLightViewProjectionMatrix(const spot_light_cb& sl);

