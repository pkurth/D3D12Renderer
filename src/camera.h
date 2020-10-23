#pragma once

#include "math.h"
#include "colliders.h"

struct camera_intrinsics
{
	float fx, fy, cx, cy;
};

struct camera_base
{
	// Camera properties.
	quat rotation;
	vec3 position;

	float nearPlane;
	float farPlane = -1.f;


	// Derived values.
	mat4 view;
	mat4 invView;

	mat4 proj;
	mat4 invProj;

	mat4 viewProj;
	mat4 invViewProj;

	ray generateWorldSpaceRay(float relX, float relY) const;
	ray generateViewSpaceRay(float relX, float relY) const;
};

struct render_camera : camera_base
{
	float verticalFOV;

	void recalculateMatrices(uint32 renderWidth, uint32 renderHeight);
};

struct real_camera : camera_base
{
	camera_intrinsics intr;
	uint32 width, height;

	void recalculateMatrices();
};
