#pragma once

#include "collision_gjk.h"

struct sat_result
{
	float minPenetration = FLT_MAX;
	vec4 plane;
};

enum sat_status
{
	sat_none,
	sat_no_intersection,
	sat_better_intersection,
	sat_worse_intersection,
};

template <typename shape_t>
static sat_status satIntersectionTest(vec4* planes, uint32 numPlanes, const shape_t& otherShape, sat_result& outResult)
{
	float minPenetration = FLT_MAX;
	uint32 faceIndex = 0;

	for (uint32 i = 0; i < numPlanes; ++i)
	{
		vec4 plane = planes[i];
		vec3 normal = plane.xyz;

		vec3 farthest = otherShape(-normal);

		float sd = signedDistanceToPlane(farthest, plane);
		if (sd > 0.f)
		{
			return sat_no_intersection;
		}

		float penetration = -sd;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			faceIndex = i;
		}
	}

	if (minPenetration < outResult.minPenetration)
	{
		outResult.minPenetration = minPenetration;
		outResult.plane = planes[faceIndex];
		return sat_better_intersection;
	}
	return sat_worse_intersection;
}

