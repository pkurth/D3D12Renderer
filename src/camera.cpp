#include "pch.h"
#include "camera.h"

void render_camera::recalculateMatrices(uint32 renderWidth, uint32 renderHeight)
{
	float aspect = (float)renderWidth / (float)renderHeight;
	proj = createPerspectiveProjectionMatrix(verticalFOV, aspect, nearPlane, farPlane);
	invProj = invertPerspectiveProjectionMatrix(proj);
	//view = createViewMatrix(position, pitch, yaw);
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
