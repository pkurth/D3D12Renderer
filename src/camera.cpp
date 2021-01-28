#include "pch.h"
#include "camera.h"

void render_camera::initializeIngame(vec3 position, quat rotation, float verticalFOV, float nearPlane, float farPlane)
{
	type = camera_type_ingame;
	this->position = position;
	this->rotation = rotation;
	this->verticalFOV = verticalFOV;
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;
}

void render_camera::initializeCalibrated(vec3 position, quat rotation, uint32 width, uint32 height, float fx, float fy, float cx, float cy, float nearPlane, float farPlane)
{
	type = camera_type_calibrated;
	this->position = position;
	this->rotation = rotation;
	this->fx = fx;
	this->fy = fy;
	this->cx = cx;
	this->cy = cy;
	this->width = width;
	this->height = height;
	this->nearPlane = nearPlane;
	this->farPlane = farPlane;
}

void render_camera::setViewport(uint32 width, uint32 height)
{
	this->width = width;
	this->height = height;
	aspect = (float)width / (float)height;
}

void render_camera::updateMatrices()
{
	if (type == camera_type_ingame)
	{
		proj = createPerspectiveProjectionMatrix(verticalFOV, aspect, nearPlane, farPlane);
	}
	else
	{
		assert(type == camera_type_calibrated);
		proj = createPerspectiveProjectionMatrix((float)width, (float)height, fx, fy, cx, cy, nearPlane, farPlane);
	}

	invProj = invertPerspectiveProjectionMatrix(proj);
	view = createViewMatrix(position, rotation);
	invView = invertedAffine(view);
	viewProj = proj * view;
	invViewProj = invView * invProj;
}

camera_projection_extents render_camera::getProjectionExtents() const
{
	if (type == camera_type_ingame)
	{
		float extentY = tanf(0.5f * verticalFOV);
		float extentX = extentY * aspect;

		return camera_projection_extents{ extentX, extentX, extentY, extentY };
	}
	else
	{
		assert(type == camera_type_calibrated);

		vec3 topLeft = restoreViewSpacePosition(vec2(0.f, 0.f), 0.f) / nearPlane;
		vec3 bottomRight = restoreViewSpacePosition(vec2(1.f, 1.f), 0.f) / nearPlane;

		return camera_projection_extents{ -topLeft.x, bottomRight.x, topLeft.y, -bottomRight.y };
	}
}

float render_camera::getMinProjectionExtent() const
{
	camera_projection_extents extents = getProjectionExtents();
	float minHorizontalExtent = min(extents.left, extents.right);
	float minVerticalExtent = min(extents.top, extents.bottom);
	float minExtent = min(minHorizontalExtent, minVerticalExtent);
	return minExtent;
}

ray render_camera::generateWorldSpaceRay(float relX, float relY) const
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

ray render_camera::generateViewSpaceRay(float relX, float relY) const
{
	float ndcX = 2.f * relX - 1.f;
	float ndcY = -(2.f * relY - 1.f);
	vec4 clip(ndcX, ndcY, -1.f, 1.f);
	vec4 eye = invProj * clip;
	eye.z = -1.f;

	ray result;
	result.origin = vec3(0.f, 0.f, 0.f);
	result.direction = normalize(eye.xyz);
	return result;
}

vec3 render_camera::restoreViewSpacePosition(vec2 uv, float depthBufferDepth) const
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
	vec4 homPosition = invProj * vec4(ndc, 1.f);
	vec3 position = homPosition.xyz / homPosition.w;
	return position;
}

vec3 render_camera::restoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const
{
	uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
	vec4 homPosition = invViewProj * vec4(ndc, 1.f);
	vec3 position = homPosition.xyz / homPosition.w;
	return position;
}

float render_camera::depthBufferDepthToEyeDepth(float depthBufferDepth) const
{
	if (farPlane < 0.f) // Infinite far plane.
	{
		depthBufferDepth = clamp(depthBufferDepth, 0.f, 1.f - 1e-7f); // A depth of 1 is at infinity.
		return -nearPlane / (depthBufferDepth - 1.f);
	}
	else
	{
		const float c1 = farPlane / nearPlane;
		const float c0 = 1.f - c1;
		return farPlane / (c0 * depthBufferDepth + c1);
	}
}

float render_camera::eyeDepthToDepthBufferDepth(float eyeDepth) const
{
	return -proj.m22 + proj.m23 / eyeDepth;
}

float render_camera::linearizeDepthBuffer(float depthBufferDepth) const
{
	if (farPlane < 0.f) // Infinite far plane.
	{
		depthBufferDepth = clamp(depthBufferDepth, 0.f, 1.f - 1e-7f); // A depth of 1 is at infinity.
		return -1.f / (depthBufferDepth - 1.f);
	}
	else
	{
		const float c1 = farPlane / nearPlane;
		const float c0 = 1.f - c1;
		return 1.f / (c0 * depthBufferDepth + c1);
	}
}

camera_frustum_corners render_camera::getWorldSpaceFrustumCorners(float alternativeFarPlane) const
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

camera_frustum_planes getWorldSpaceFrustumPlanes(const mat4& viewProj)
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

camera_frustum_planes render_camera::getWorldSpaceFrustumPlanes() const
{
	return ::getWorldSpaceFrustumPlanes(viewProj);
}

camera_frustum_corners render_camera::getViewSpaceFrustumCorners(float alternativeFarPlane) const
{
	if (alternativeFarPlane <= 0.f)
	{
		alternativeFarPlane = farPlane;
	}

	float depthValue = eyeDepthToDepthBufferDepth(alternativeFarPlane);

	camera_frustum_corners result;

	result.eye = position;

	result.nearBottomLeft = restoreViewSpacePosition(vec2(0.f, 1.f), 0.f);
	result.nearBottomRight = restoreViewSpacePosition(vec2(1.f, 1.f), 0.f);
	result.nearTopLeft = restoreViewSpacePosition(vec2(0.f, 0.f), 0.f);
	result.nearTopRight = restoreViewSpacePosition(vec2(1.f, 0.f), 0.f);
	result.farBottomLeft = restoreViewSpacePosition(vec2(0.f, 1.f), depthValue);
	result.farBottomRight = restoreViewSpacePosition(vec2(1.f, 1.f), depthValue);
	result.farTopLeft = restoreViewSpacePosition(vec2(0.f, 0.f), depthValue);
	result.farTopRight = restoreViewSpacePosition(vec2(1.f, 0.f), depthValue);

	return result;
}

render_camera render_camera::getJitteredVersion(vec2 offset) const
{
	camera_projection_extents extents = getProjectionExtents();
	float texelSizeX = (extents.left + extents.right) / width;
	float texelSizeY = (extents.top + extents.bottom) / height;

	float jitterX = texelSizeX * offset.x;
	float jitterY = texelSizeY * offset.y;

	float left = jitterX - extents.left;
	float right = jitterX + extents.right;
	float bottom = jitterY - extents.bottom;
	float top = jitterY + extents.top;

	mat4 jitteredProj = createPerspectiveProjectionMatrix(right * nearPlane, left * nearPlane, top * nearPlane, bottom * nearPlane, nearPlane, farPlane);

	render_camera result = *this;

	result.proj = jitteredProj;
	result.invProj = invertPerspectiveProjectionMatrix(jitteredProj);
	result.viewProj = jitteredProj * view;
	result.invViewProj = invView * result.invProj;

	return result;
}

bool camera_frustum_planes::cullWorldSpaceAABB(const bounding_box& aabb) const
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

bool camera_frustum_planes::cullModelSpaceAABB(const bounding_box& aabb, const trs& transform) const
{
	return cullModelSpaceAABB(aabb, trsToMat4(transform));
}

bool camera_frustum_planes::cullModelSpaceAABB(const bounding_box& aabb, const mat4& transform) const
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
