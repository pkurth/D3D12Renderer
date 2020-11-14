#include "pch.h"
#include "camera.h"

void render_camera::recalculateMatrices(uint32 renderWidth, uint32 renderHeight)
{
	return recalculateMatrices((float)renderWidth, (float)renderHeight);
}

void render_camera::recalculateMatrices(float renderWidth, float renderHeight)
{
	float aspect = renderWidth / renderHeight;
	proj = createPerspectiveProjectionMatrix(verticalFOV, aspect, nearPlane, farPlane);
	invProj = invertPerspectiveProjectionMatrix(proj);
	view = createViewMatrix(position, rotation);
	invView = invertedAffine(view);
	viewProj = proj * view;
	invViewProj = invView * invProj;
}

ray camera_base::generateWorldSpaceRay(float relX, float relY) const
{
	float ndcX = 2.f * relX - 1.f;
	float ndcY = -(2.f * relY - 1.f);
	vec4 clip(ndcX, ndcY, -1.f, 1.f);
	vec4 eye = invProj * clip;
	eye.z = -1.f; eye.w = 0.f;

	ray result;
	result.origin = position;
	result.direction = normalize((invView * eye).xyz);
	return result;
}

ray camera_base::generateViewSpaceRay(float relX, float relY) const
{
	float ndcX = 2.f * relX - 1.f;
	float ndcY = -(2.f * relY - 1.f);
	vec4 clip(ndcX, ndcY, -1.f, 1.f);
	vec4 eye = invProj * clip;
	eye.z = -1.f; eye.w = 0.f;

	ray result;
	result.origin = vec3(0.f, 0.f, 0.f);
	result.direction = normalize(eye.xyz);
	return result;
}

vec3 camera_base::restoreViewSpacePosition(vec2 uv, float depthBufferDepth) const
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
	vec4 homPosition = invProj * vec4(ndc, 1.f);
	vec3 position = homPosition.xyz / homPosition.w;
	return position;
}

vec3 camera_base::restoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
	vec4 homPosition = invViewProj * vec4(ndc, 1.f);
	vec3 position = homPosition.xyz / homPosition.w;
	return position;
}

float camera_base::depthBufferDepthToEyeDepth(float depthBufferDepth) const
{
	if (farPlane < 0.f) // Infinite far plane.
	{
		depthBufferDepth = clamp(depthBufferDepth, 0.f, 1.f - 1e-7f); // A depth of 1 is at infinity.
		return -nearPlane / (depthBufferDepth - 1.f);
	}
	else
	{
		const float c1 = farPlane / nearPlane;
		const float c0 = 1.f - farPlane / nearPlane;
		return farPlane / (c0 * depthBufferDepth + c1);
	}
}

float camera_base::eyeDepthToDepthBufferDepth(float eyeDepth) const
{
	return -proj.m22 + proj.m23 / eyeDepth;
}

camera_frustum_corners camera_base::getWorldSpaceFrustumCorners(float alternativeFarPlane) const
{
	if (alternativeFarPlane <= 0.f)
	{
		alternativeFarPlane = farPlane;
	}

	float depthValue = eyeDepthToDepthBufferDepth(alternativeFarPlane);

	camera_frustum_corners result;

	result.eye = position;

	result.nearBottomLeft = restoreWorldSpacePosition(vec2(0.f, 1.f), 0.f);
	result.nearBottomRight = restoreWorldSpacePosition(vec2(1.f, 1.f), 0.f);
	result.nearTopLeft = restoreWorldSpacePosition(vec2(0.f, 0.f), 0.f);
	result.nearTopRight = restoreWorldSpacePosition(vec2(1.f, 0.f), 0.f);
	result.farBottomLeft = restoreWorldSpacePosition(vec2(0.f, 1.f), depthValue);
	result.farBottomRight = restoreWorldSpacePosition(vec2(1.f, 1.f), depthValue);
	result.farTopLeft = restoreWorldSpacePosition(vec2(0.f, 0.f), depthValue);
	result.farTopRight = restoreWorldSpacePosition(vec2(1.f, 0.f), depthValue);

	return result;
}

camera_frustum_planes camera_base::getWorldSpaceFrustumPlanes() const
{
	camera_frustum_planes result;

	vec4 c0(viewProj.m00, viewProj.m01, viewProj.m02, viewProj.m03);
	vec4 c1(viewProj.m10, viewProj.m11, viewProj.m12, viewProj.m13);
	vec4 c2(viewProj.m20, viewProj.m21, viewProj.m22, viewProj.m23);
	vec4 c3(viewProj.m30, viewProj.m31, viewProj.m32, viewProj.m33);

	result.leftPlane = c3 + c0;
	result.rightPlane = c3 - c0;
	result.topPlane = c3 - c1;
	result.bottomPlane = c3 + c1;
	result.nearPlane = c2;
	result.farPlane = c3 - c2;

	return result;
}

void real_camera::recalculateMatrices()
{
	float aspect = (float)width / (float)height;
	proj = createPerspectiveProjectionMatrix((float)width, (float)height, intr.fx, intr.fy, intr.cx, intr.cy, nearPlane, farPlane);
	invProj = invertPerspectiveProjectionMatrix(proj);
	view = createViewMatrix(position, rotation);
	invView = invertedAffine(view);
	viewProj = proj * view;
	invViewProj = invView * invProj;
}

bool camera_frustum_planes::cullWorldSpaceAABB(const aabb_collider& aabb) const
{
	for (uint32 i = 0; i < 6; ++i)
	{
		vec4 plane = planes[i];
		vec4 vertex(
			(plane.x < 0.f) ? aabb.minCorner.x : aabb.maxCorner.x,
			(plane.y < 0.f) ? aabb.minCorner.y : aabb.maxCorner.y,
			(plane.z < 0.f) ? aabb.minCorner.z : aabb.maxCorner.z,
			1.f
		);
		if (dot(plane, vertex) < 0.f)
		{
			return true;
		}
	}
	return false;
}

bool camera_frustum_planes::cullModelSpaceAABB(const aabb_collider& aabb, const trs& transform) const
{
	return cullModelSpaceAABB(aabb, trsToMat4(transform));
}

bool camera_frustum_planes::cullModelSpaceAABB(const aabb_collider& aabb, const mat4& transform) const
{
	// TODO: Transform planes instead of AABB?

	vec4 worldSpaceCorners[] =
	{
		transform * vec4(aabb.minCorner.x, aabb.minCorner.y, aabb.minCorner.z, 1.f),
		transform * vec4(aabb.maxCorner.x, aabb.minCorner.y, aabb.minCorner.z, 1.f),
		transform * vec4(aabb.minCorner.x, aabb.maxCorner.y, aabb.minCorner.z, 1.f),
		transform * vec4(aabb.maxCorner.x, aabb.maxCorner.y, aabb.minCorner.z, 1.f),
		transform * vec4(aabb.minCorner.x, aabb.minCorner.y, aabb.maxCorner.z, 1.f),
		transform * vec4(aabb.maxCorner.x, aabb.minCorner.y, aabb.maxCorner.z, 1.f),
		transform * vec4(aabb.minCorner.x, aabb.maxCorner.y, aabb.maxCorner.z, 1.f),
		transform * vec4(aabb.maxCorner.x, aabb.maxCorner.y, aabb.maxCorner.z, 1.f),
	};

	for (uint32 i = 0; i < 6; ++i)
	{
		vec4 plane = planes[i];

		bool inside = false;

		for (uint32 j = 0; j < 8; ++j)
		{
			if (dot(plane, worldSpaceCorners[j]) > 0.f)
			{
				inside = true;
				break;
			}
		}

		if (!inside)
		{
			return true;
		}
	}

	return false;
}
