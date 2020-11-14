#pragma once

#include "math.h"
#include "colliders.h"

union camera_frustum_corners
{
	vec3 eye;

	struct
	{
		vec3 nearTopLeft;
		vec3 nearTopRight;
		vec3 nearBottomLeft;
		vec3 nearBottomRight;
		vec3 farTopLeft;
		vec3 farTopRight;
		vec3 farBottomLeft;
		vec3 farBottomRight;
	};
	struct
	{
		vec3 corners[8];
	};

	camera_frustum_corners() {}
};

union camera_frustum_planes
{
	struct
	{
		vec4 nearPlane;
		vec4 farPlane;
		vec4 leftPlane;
		vec4 rightPlane;
		vec4 topPlane;
		vec4 bottomPlane;
	};
	vec4 planes[6];

	camera_frustum_planes() {}

	// Returns true, if object should be culled.
	bool cullWorldSpaceAABB(const aabb_collider& aabb) const;
	bool cullModelSpaceAABB(const aabb_collider& aabb, const trs& transform) const;
	bool cullModelSpaceAABB(const aabb_collider& aabb, const mat4& transform) const;
};


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

	vec3 restoreViewSpacePosition(vec2 uv, float depthBufferDepth) const;
	vec3 restoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const;
	float depthBufferDepthToEyeDepth(float depthBufferDepth) const;
	float eyeDepthToDepthBufferDepth(float eyeDepth) const;

	camera_frustum_corners getWorldSpaceFrustumCorners(float alternativeFarPlane = 0.f) const;
	camera_frustum_planes getWorldSpaceFrustumPlanes() const;
};

struct render_camera : camera_base
{
	float verticalFOV;

	void recalculateMatrices(uint32 renderWidth, uint32 renderHeight);
	void recalculateMatrices(float renderWidth, float renderHeight);
};

struct real_camera : camera_base
{
	camera_intrinsics intr;
	uint32 width, height;

	void recalculateMatrices();
};
