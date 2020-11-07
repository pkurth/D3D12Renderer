#pragma once

#include "math.h"
#include "camera.h"

#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f

struct light_attenuation
{
	float constant = 1.f;
	float linear;
	float quadratic;


	float getAttenuation(float distance)
	{
		return 1.f / (constant + linear * distance + quadratic * distance * distance);
	}

	float getMaxDistance(float lightMax)
	{
		return (-linear + sqrtf(linear * linear - 4.f * quadratic * (constant - (256.f / 5.f) * lightMax)))
			/ (2.f * quadratic);
	}
};

struct directional_light
{
	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];
	vec4 cascadeDistances;
	vec4 bias;

	vec4 worldSpaceDirection;
	vec4 color;

	uint32 numShadowCascades = 3;
	float blendArea;
	float texelSize;
	uint32 shadowMapDimensions = 2048;

	void updateMatrices(const render_camera& camera);
};

struct spot_light
{
	mat4 vp;

	vec4 worldSpacePosition;
	vec4 worldSpaceDirection;
	vec4 color;

	light_attenuation attenuation;

	float innerAngle;
	float outerAngle;
	float innerCutoff; // cos(innerAngle).
	float outerCutoff; // cos(outerAngle).
	float texelSize;
	float bias;
	uint32 shadowMapDimensions = 2048;

	void updateMatrices();
};

struct point_light
{
	vec4 worldSpacePositionAndRadius;
	vec4 color;
};
