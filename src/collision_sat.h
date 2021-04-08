#pragma once

#include "collision_gjk.h"

struct sat_result
{
	float minPenetration = FLT_MAX;
	vec4 plane;

	uint32 faceIndex;
	uint32 edgeIndexA;
	uint32 edgeIndexB;
};

enum sat_status
{
	sat_none,
	sat_no_intersection,
	sat_better_intersection,
	sat_worse_intersection,
};

struct sat_edge
{
	vec3 start;
	vec3 direction;
};

template <typename shape_t>
static sat_status satFaceIntersectionTest(vec4* planes, uint32 numPlanes, const shape_t& otherShape, sat_result& outResult)
{
	float minPenetration = outResult.minPenetration;
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
		outResult.faceIndex = faceIndex;
		return sat_better_intersection;
	}
	return sat_worse_intersection;
}

static bool isMinkowskiFace(vec3 a, vec3 b, vec3 c, vec3 d)
{
	vec3 bxa = cross(b, a);
	vec3 dxc = cross(d, c);

	float cba = dot(c, bxa);
	float dba = dot(d, bxa);
	float adc = dot(a, dxc);
	float bdc = dot(b, dxc);

	return cba * dba < 0.f && adc * bdc < 0.f && cba * bdc > 0.f;
}

template <typename shape_t>
static sat_status satEdgeIntersectionTest(sat_edge* edgesA, uint32 numEdgesA, vec3 centerA, sat_edge* edgesB, uint32 numEdgesB, const shape_t& shapeB, sat_result& outResult)
{
	float minPenetration = outResult.minPenetration;
	vec4 minPlane;
	uint32 edgeIndexA = 0;
	uint32 edgeIndexB = 0;

	for (uint32 aEdgeIndex = 0; aEdgeIndex < numEdgesA; ++aEdgeIndex)
	{
		sat_edge& aEdge = edgesA[aEdgeIndex];
		vec3 fromCenterA = aEdge.start - centerA;

		for (uint32 bEdgeIndex = 0; bEdgeIndex < numEdgesB; ++bEdgeIndex)
		{
			sat_edge& bEdge = edgesB[bEdgeIndex];

			vec3 axis = cross(aEdge.direction, bEdge.direction);
			if (dot(axis, fromCenterA) < 0.f)
			{
				axis = -axis;
			}

			float sqLength = squaredLength(axis);
			if (sqLength < 0.001f)
			{
				continue;
			}

			vec4 plane = createPlane(aEdge.start, axis / sqrt(sqLength));
			vec3 normal = plane.xyz;

			vec3 farthest = shapeB(-normal);

			float sd = signedDistanceToPlane(farthest, plane);
			if (sd > 0.f)
			{
				return sat_no_intersection;
			}

			float penetration = -sd;
			if (penetration < minPenetration)
			{
				minPenetration = penetration;
				minPlane = plane;
				edgeIndexA = aEdgeIndex;
				edgeIndexB = bEdgeIndex;
			}
		}
	}

	if (minPenetration < outResult.minPenetration)
	{
		outResult.minPenetration = minPenetration;
		outResult.plane = minPlane;
		outResult.edgeIndexA = edgeIndexA;
		outResult.edgeIndexB = edgeIndexB;
		return sat_better_intersection;
	}
	return sat_worse_intersection;
}

