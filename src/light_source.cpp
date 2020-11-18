#include "pch.h"
#include "light_source.h"


void directional_light::updateMatrices(const render_camera& camera)
{
	mat4 viewMatrix = lookAt(vec3(0.f, 0.f, 0.f), direction, vec3(0.f, 1.f, 0.f));

	vec4 worldForward(camera.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	camera_frustum_corners worldFrustum = camera.getWorldSpaceFrustumCorners(cascadeDistances.w);

	vec4 worldBottomLeft(worldFrustum.farBottomLeft - worldFrustum.nearBottomLeft, 0.f);
	vec4 worldBottomRight(worldFrustum.farBottomRight - worldFrustum.nearBottomRight, 0.f);
	vec4 worldTopLeft(worldFrustum.farTopLeft - worldFrustum.nearTopLeft, 0.f);
	vec4 worldTopRight(worldFrustum.farTopRight - worldFrustum.nearTopRight, 0.f);

	worldBottomLeft /= dot(worldBottomLeft, worldForward);
	worldBottomRight /= dot(worldBottomRight, worldForward);
	worldTopLeft /= dot(worldTopLeft, worldForward);
	worldTopRight /= dot(worldTopRight, worldForward);

	vec4 worldEye = vec4(camera.position, 1.f);
	vec4 sunEye = viewMatrix * worldEye;

	bounding_box initialBB = bounding_box::fromMinMax(sunEye.xyz, sunEye.xyz);

	for (uint32 i = 0; i < numShadowCascades; ++i)
	{
		float distance = cascadeDistances.data[i];

		vec4 sunBottomLeft = viewMatrix * (worldEye + distance * worldBottomLeft);
		vec4 sunBottomRight = viewMatrix * (worldEye + distance * worldBottomRight);
		vec4 sunTopLeft = viewMatrix * (worldEye + distance * worldTopLeft);
		vec4 sunTopRight = viewMatrix * (worldEye + distance * worldTopRight);

		bounding_box bb = initialBB;
		bb.grow(sunBottomLeft.xyz);
		bb.grow(sunBottomRight.xyz);
		bb.grow(sunTopLeft.xyz);
		bb.grow(sunTopRight.xyz);

		bb.pad(vec3(2.f, 2.f, 2.f));

		mat4 projMatrix = createOrthographicProjectionMatrix(bb.minCorner.x, bb.maxCorner.x, bb.maxCorner.y, bb.minCorner.y, -bb.maxCorner.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -bb.minCorner.z);

		vp[i] = projMatrix * viewMatrix;
	}
}
