#pragma once

#include "math.h"
#include "camera.h"

#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f


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

