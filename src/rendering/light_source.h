#pragma once

#include "core/math.h"
#include "core/camera.h"
#include "core/reflect.h"

#include "light_source.hlsli"

struct directional_light
{
	vec3 color;
	float intensity; // Final radiance is color * intensity.

	vec3 direction;
	uint32 numShadowCascades;

	vec4 cascadeDistances;
	vec4 bias;

	vec4 shadowMapViewports[MAX_NUM_SUN_SHADOW_CASCADES];
	mat4 viewProjs[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 blendDistances;
	uint32 shadowDimensions = 2048;

	float negativeZOffset = 500.f;

	bool stabilize;

	void updateMatrices(const render_camera& camera);
};
REFLECT_STRUCT(directional_light,
	(color, "Color"),
	(intensity, "Intensity"),
	(direction, "Direction"),
	(numShadowCascades, "Cascades"),
	(cascadeDistances, "Cascade distances"),
	(bias, "Bias"),
	(blendDistances, "Blend distances"),
	(shadowDimensions, "Shadow dimensions"),
	(stabilize, "Stabilize")
);

struct point_light_component
{
	vec3 color;
	float intensity; // Final radiance is color * intensity.
	float radius;
	bool castsShadow;
	uint32 shadowMapResolution;

	point_light_component() {}
	point_light_component(vec3 color, float intensity, float radius, bool castsShadow = false, uint32 shadowMapResolution = 512)
		: color(color), intensity(intensity), radius(radius), castsShadow(castsShadow), shadowMapResolution(shadowMapResolution) {}
	point_light_component(const point_light_component&) = default;
};
REFLECT_STRUCT(point_light_component,
	(color, "Color"),
	(intensity, "Intensity"),
	(radius, "Radius"),
	(castsShadow, "Casts shadow"),
	(shadowMapResolution, "Shadow resolution")
);

struct spot_light_component
{
	vec3 color;
	float intensity; // Final radiance is color * intensity.
	float distance;
	float innerAngle;
	float outerAngle;
	bool castsShadow;
	uint32 shadowMapResolution;

	spot_light_component() {}
	spot_light_component(vec3 color, float intensity, float distance, float innerAngle, float outerAngle, bool castsShadow = false, uint32 shadowMapResolution = 512)
		: color(color), intensity(intensity), distance(distance), innerAngle(innerAngle), outerAngle(outerAngle), castsShadow(castsShadow), shadowMapResolution(shadowMapResolution) {}
	spot_light_component(const spot_light_component&) = default;
};
REFLECT_STRUCT(spot_light_component,
	(color, "Color"),
	(intensity, "Intensity"),
	(distance, "Distance"),
	(innerAngle, "Inner angle"),
	(outerAngle, "Outer angle"),
	(castsShadow, "Casts shadow"),
	(shadowMapResolution, "Shadow resolution")
);

mat4 getSpotLightViewProjectionMatrix(const spot_light_cb& sl);

