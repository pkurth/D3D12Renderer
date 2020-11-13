#pragma once

#include "math.h"
#include "camera.h"

#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f


//struct directional_light
//{
//	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];
//	vec4 cascadeDistances;
//	vec4 bias;
//
//	vec4 worldSpaceDirection;
//	vec4 color;
//
//	uint32 numShadowCascades = 3;
//	float blendArea;
//	float texelSize;
//	uint32 shadowMapDimensions = 2048;
//
//	void updateMatrices(const render_camera& camera);
//};
//
//struct spot_light
//{
//	mat4 vp;
//
//	vec4 worldSpacePosition;
//	vec4 worldSpaceDirection;
//	vec4 color;
//
//	light_attenuation attenuation;
//
//	float innerAngle;
//	float outerAngle;
//	float innerCutoff; // cos(innerAngle).
//	float outerCutoff; // cos(outerAngle).
//	float texelSize;
//	float bias;
//	uint32 shadowMapDimensions = 2048;
//
//	void updateMatrices();
//};
//
//struct point_light
//{
//	vec4 worldSpacePositionAndRadius;
//	vec4 color;
//};
