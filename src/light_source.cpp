#include "pch.h"
#include "light_source.h"


void directional_light::updateMatrices(const render_camera& camera)
{
	camera_frustum_corners worldFrustum = camera.getWorldSpaceFrustumCorners(cascadeDistances.w);

	vec3 bottomLeftRay = worldFrustum.farBottomLeft - worldFrustum.nearBottomLeft;
	vec3 bottomRightRay = worldFrustum.farBottomRight - worldFrustum.nearBottomRight;
	vec3 topLeftRay = worldFrustum.farTopLeft - worldFrustum.nearTopLeft;
	vec3 topRightRay = worldFrustum.farTopRight - worldFrustum.nearTopRight;

	vec3 cameraForward = camera.rotation * vec3(0.f, 0.f, -1.f);

	bottomLeftRay /= dot(bottomLeftRay, cameraForward);
	bottomRightRay /= dot(bottomRightRay, cameraForward);
	topLeftRay /= dot(topLeftRay, cameraForward);
	topRightRay /= dot(topRightRay, cameraForward);

	for (uint32 cascade = 0; cascade < numShadowCascades; ++cascade)
	{
		float distance = cascadeDistances.data[cascade];
		float prevDistance = (cascade == 0) ? camera.nearPlane : cascadeDistances.data[cascade - 1];

		vec3 corners[8];
		corners[0] = camera.position + bottomLeftRay * prevDistance;
		corners[1] = camera.position + bottomRightRay * prevDistance;
		corners[2] = camera.position + topLeftRay * prevDistance;
		corners[3] = camera.position + topRightRay * prevDistance;
		corners[4] = camera.position + bottomLeftRay * distance;
		corners[5] = camera.position + bottomRightRay * distance;
		corners[6] = camera.position + topLeftRay * distance;
		corners[7] = camera.position + topRightRay * distance;

		vec3 center(0.f);
		for (uint32 i = 0; i < 8; ++i)
		{
			center += corners[i];
		}
		center /= 8.f;

		vec3 upDir = stabilize ? vec3(0.f, 1.f, 0.f) : (camera.rotation * vec3(1.f, 0.f, 1.f));

		mat4 viewMatrix = lookAt(center, center + direction, upDir);

		vec3 minExtents, maxExtents;
		if (stabilize)
		{
			float sphereRadius = 0.f;
			for (uint32 i = 0; i < 8; ++i)
			{
				float d = length(corners[i] - center);
				sphereRadius = max(sphereRadius, d);
			}

			sphereRadius = ceil(sphereRadius * 16.f) / 16.f;
			minExtents = -sphereRadius;
			maxExtents = sphereRadius;
		}
		else
		{
			bounding_box extents = bounding_box::negativeInfinity();
			for (uint32 i = 0; i < 8; ++i)
			{
				vec3 c = transformPosition(viewMatrix, corners[i]);
				extents.grow(c);
			}

			minExtents = extents.minCorner;
			maxExtents = extents.maxCorner;

			float scale = (shadowDimensions + 9.f) / shadowDimensions;
			minExtents.xy *= scale;
			maxExtents.xy *= scale;
		}

		vec3 cascadeExtents = maxExtents - minExtents;
		vec3 shadowCamPos = center + direction * minExtents.z;

		viewMatrix = lookAt(shadowCamPos, shadowCamPos + direction, upDir);

		mat4 projMatrix = createOrthographicProjectionMatrix(maxExtents.x, minExtents.x, maxExtents.y, minExtents.y, -SHADOW_MAP_NEGATIVE_Z_OFFSET, cascadeExtents.z);

		if (stabilize)
		{
			mat4 matrix = projMatrix * viewMatrix;
			vec3 shadowOrigin(0.f);
			shadowOrigin = (matrix * vec4(shadowOrigin, 1.f)).xyz;
			shadowOrigin *= (shadowDimensions * 0.5f);

			vec3 roundedOrigin = round(shadowOrigin);
			vec3 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * (2.f / shadowDimensions);

			projMatrix.m03 += roundOffset.x;
			projMatrix.m13 += roundOffset.y;
		}

		vp[cascade] = projMatrix * viewMatrix;
	}
}

mat4 getSpotLightViewProjectionMatrix(const spot_light_cb& sl)
{
	mat4 viewMatrix = lookAt(sl.position, sl.position + sl.direction, vec3(0.f, 1.f, 0.f));
	mat4 projMatrix = createPerspectiveProjectionMatrix(acos(sl.getOuterCutoff()) * 2.f, 1.f, 0.01f, sl.maxDistance);
	return projMatrix * viewMatrix;
}
