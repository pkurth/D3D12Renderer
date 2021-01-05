#include "pch.h"
#include "light_source.h"


void directional_light::updateMatrices(const render_camera& camera, bool preventRotationalShimmering)
{
	mat4 viewMatrix = lookAt(vec3(0.f, 0.f, 0.f), direction, vec3(0.f, 1.f, 0.f));

	camera_frustum_corners viewFrustum = camera.getViewSpaceFrustumCorners(cascadeDistances.w);

	// View space.
	vec4 viewBottomLeft(viewFrustum.farBottomLeft - viewFrustum.nearBottomLeft, 0.f);
	vec4 viewBottomRight(viewFrustum.farBottomRight - viewFrustum.nearBottomRight, 0.f);
	vec4 viewTopLeft(viewFrustum.farTopLeft - viewFrustum.nearTopLeft, 0.f);
	vec4 viewTopRight(viewFrustum.farTopRight - viewFrustum.nearTopRight, 0.f);

	// Normalize to z == -1.
	viewBottomLeft /= -viewBottomLeft.z;
	viewBottomRight /= -viewBottomRight.z;
	viewTopLeft /= -viewTopLeft.z;
	viewTopRight /= -viewTopRight.z;

	bounding_box initialViewSpaceBB = bounding_box::negativeInfinity();
	initialViewSpaceBB.grow((camera.nearPlane * viewBottomLeft).xyz);
	initialViewSpaceBB.grow((camera.nearPlane * viewBottomRight).xyz);
	initialViewSpaceBB.grow((camera.nearPlane * viewTopLeft).xyz);
	initialViewSpaceBB.grow((camera.nearPlane * viewTopRight).xyz);


	// World space.
	vec4 worldBottomLeft = vec4(camera.rotation * viewBottomLeft.xyz, 0.f);
	vec4 worldBottomRight = vec4(camera.rotation * viewBottomRight.xyz, 0.f);
	vec4 worldTopLeft = vec4(camera.rotation * viewTopLeft.xyz, 0.f);
	vec4 worldTopRight = vec4(camera.rotation * viewTopRight.xyz, 0.f);

	vec4 worldEye = vec4(camera.position, 1.f);

	bounding_box initialWorldSpaceBB = bounding_box::negativeInfinity();
	initialWorldSpaceBB.grow((viewMatrix * (worldEye + camera.nearPlane * worldBottomLeft)).xyz);
	initialWorldSpaceBB.grow((viewMatrix * (worldEye + camera.nearPlane * worldBottomRight)).xyz);
	initialWorldSpaceBB.grow((viewMatrix * (worldEye + camera.nearPlane * worldTopLeft)).xyz);
	initialWorldSpaceBB.grow((viewMatrix * (worldEye + camera.nearPlane * worldTopRight)).xyz);


	for (uint32 i = 0; i < numShadowCascades; ++i)
	{
		float distance = cascadeDistances.data[i];

		bounding_box worldBB = initialWorldSpaceBB;
		worldBB.grow((viewMatrix * (worldEye + distance * worldBottomLeft)).xyz);
		worldBB.grow((viewMatrix * (worldEye + distance * worldBottomRight)).xyz);
		worldBB.grow((viewMatrix * (worldEye + distance * worldTopLeft)).xyz);
		worldBB.grow((viewMatrix * (worldEye + distance * worldTopRight)).xyz);
		worldBB.pad(0.1f);

		mat4 projMatrix;

		if (!preventRotationalShimmering)
		{
			projMatrix = createOrthographicProjectionMatrix(worldBB.minCorner.x, worldBB.maxCorner.x, worldBB.maxCorner.y, worldBB.minCorner.y,
				-worldBB.maxCorner.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -worldBB.minCorner.z);
		}
		else
		{
			bounding_box viewBB = initialViewSpaceBB;
			viewBB.grow((distance * viewBottomLeft).xyz);
			viewBB.grow((distance * viewBottomRight).xyz);
			viewBB.grow((distance * viewTopLeft).xyz);
			viewBB.grow((distance * viewTopRight).xyz);
			viewBB.pad(0.1f);

			vec3 viewSpaceCenter = viewBB.getCenter();
			float radius = length(viewBB.getRadius());

			vec3 center = (viewMatrix * vec4(camera.rotation * viewSpaceCenter + camera.position, 1.f)).xyz;

			projMatrix = createOrthographicProjectionMatrix(center.x + radius, center.x - radius, center.y + radius, center.y - radius,
				-worldBB.maxCorner.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -worldBB.minCorner.z);
		}

		vp[i] = projMatrix * viewMatrix;

		// Move in pixel increments.
		// https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering

		vec4 shadowOrigin = (vp[i] * vec4(0.f, 0.f, 0.f, 1.f)) * SUN_SHADOW_DIMENSIONS * 0.5f;
		vec4 roundedOrigin = round(shadowOrigin);
		vec4 roundOffset = roundedOrigin - shadowOrigin;
		roundOffset = roundOffset * 2.f / SUN_SHADOW_DIMENSIONS;
		roundOffset.z = 0.f;
		roundOffset.w = 0.f;

		vp[i].col3 += roundOffset;
	}
}
