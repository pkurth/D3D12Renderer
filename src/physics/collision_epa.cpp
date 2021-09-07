#include "pch.h"
#include "collision_epa.h"


epa_triangle_info epa_simplex::getTriangleInfo(const gjk_support_point& a, const gjk_support_point& b, const gjk_support_point& c)
{
	epa_triangle_info result;
	result.normal = normalize(cross(b.minkowski - a.minkowski, c.minkowski - a.minkowski));
	result.distanceToOrigin = dot(result.normal, a.minkowski);
	return result;
}

bool epa_simplex::isTriangleActive(uint32 index)
{
	uint32 wordIndex = index / 32;
	uint32 bitIndex = index % 32;

	return (activeTrianglesMask[wordIndex] & (1 << bitIndex)) != 0;
}

void epa_simplex::setTriangleActive(uint32 index)
{
	assert(!isTriangleActive(index));

	uint32 wordIndex = index / 32;
	uint32 bitIndex = index % 32;

	activeTrianglesMask[wordIndex] |= (1 << bitIndex);
}

void epa_simplex::setTriangleInactive(uint32 index)
{
	assert(isTriangleActive(index));

	uint32 wordIndex = index / 32;
	uint32 bitIndex = index % 32;

	activeTrianglesMask[wordIndex] &= ~(1 << bitIndex);
}

uint16 epa_simplex::pushPoint(const gjk_support_point& a)
{
	if (numPoints >= arraysize(points))
	{
		return UINT16_MAX;
	}

	uint16 index = numPoints++;
	points[index] = a;
	return index;
}

uint16 epa_simplex::pushTriangle(uint16 a, uint16 b, uint16 c, uint16 edgeOppositeA, uint16 edgeOppositeB, uint16 edgeOppositeC, epa_triangle_info info)
{
	if (numTriangles >= arraysize(triangles))
	{
		return UINT16_MAX;
	}

	uint16 index = numTriangles++;
	setTriangleActive(index);

	epa_triangle& tri = triangles[index];
	tri.a = a;
	tri.b = b;
	tri.c = c;
	tri.edgeOppositeA = edgeOppositeA;
	tri.edgeOppositeB = edgeOppositeB;
	tri.edgeOppositeC = edgeOppositeC;
	tri.normal = info.normal;
	tri.distanceToOrigin = info.distanceToOrigin;
		
	return index;
}

uint16 epa_simplex::pushEdge(uint16 a, uint16 b, uint16 triangleA, uint16 triangleB)
{
	if (numEdges >= arraysize(edges))
	{
		return UINT16_MAX;
	}

	uint16 index = numEdges++;
	epa_edge& edge = edges[index];
	edge.a = a;
	edge.b = b;
	edge.triangleA = triangleA;
	edge.triangleB = triangleB;
	return index;
}

uint32 epa_simplex::findTriangleClosestToOrigin()
{
	uint32 closest = -1;
	float minDistance = FLT_MAX;
	for (uint32 i = 0; i < numTriangles; ++i)
	{
		if (isTriangleActive(i))
		{
			epa_triangle& tri = triangles[i];
			if (tri.distanceToOrigin < minDistance)
			{
				minDistance = tri.distanceToOrigin;
				closest = i;
			}
		}
	}

	assert(closest != -1);

	return closest;
}

bool epa_simplex::addNewPointAndUpdate(const gjk_support_point& newPoint)
{
	// This function removes all triangles which point towards the new point and replaces them with a triangle fan connecting the new point to the simplex.

	uint8 edgeReferences[arraysize(edges)] = { 0 };

	for (uint32 i = 0; i < numTriangles; ++i)
	{
		if (isTriangleActive(i))
		{
			epa_triangle& tri = triangles[i];
			float d = dot(tri.normal, newPoint.minkowski - points[tri.a].minkowski);
			if (d > 0.f)
			{
				// Remove triangle and mark edges.
#define REFERENCE_EDGE(e) assert(e < numEdges); ++edgeReferences[e];

				REFERENCE_EDGE(tri.edgeOppositeA);
				REFERENCE_EDGE(tri.edgeOppositeB);
				REFERENCE_EDGE(tri.edgeOppositeC);

				setTriangleInactive(i);
#undef REFERENCE_EDGE
			}
		}
	}

	uint16 borderEdgeIndices[128];
	uint32 numBorderEdges = 0;

	for (uint32 i = 0; i < numEdges; ++i)
	{
		assert(edgeReferences[i] <= 2);

		if (edgeReferences[i] == 1)
		{
			if (numBorderEdges >= arraysize(borderEdgeIndices))
			{
				return false;
			}
			borderEdgeIndices[numBorderEdges++] = i;
		}
	}

	assert(numBorderEdges > 0);

	uint16 newEdgePerPoint[arraysize(points)];

	uint16 newPointIndex = pushPoint(newPoint);
	if (newPointIndex == UINT16_MAX)
	{
		return false;
	}

	uint16 triangleOffset = numTriangles;

	for (uint32 i = 0; i < numBorderEdges; ++i)
	{
		// Add a triangle for each border edge connecting it to the new point.

		uint16 edgeIndex = borderEdgeIndices[i];
		epa_edge& edge = edges[edgeIndex];

		bool triAActive = isTriangleActive(edge.triangleA);
		bool triBActive = isTriangleActive(edge.triangleB);
		assert(triAActive != triBActive);

		uint16 pointToConnect = triBActive ? edge.a : edge.b;

		uint16 triangleIndex = numTriangles;

		// Push edge from border edge start to new point. The other edge will be added later, which is why we set its index to -1 here temporarily.
		uint16 newEdgeIndex = pushEdge(pointToConnect, newPointIndex, -1, numTriangles);
		if (newEdgeIndex == UINT16_MAX)
		{
			return false;
		}
		newEdgePerPoint[pointToConnect] = newEdgeIndex;


		uint16 bIndex = pointToConnect;
		uint16 cIndex = triBActive ? edge.b : edge.a;

		const gjk_support_point& b = points[bIndex];
		const gjk_support_point& c = points[cIndex];

		// Index of edge opposite B is again -1.
		uint16 triangleIndexTest = pushTriangle(newPointIndex, bIndex, cIndex, edgeIndex, -1, newEdgeIndex, getTriangleInfo(newPoint, b, c));
		if (triangleIndexTest == UINT16_MAX)
		{
			return false;
		}
		assert(triangleIndex == triangleIndexTest);

		// Set edge's new neighbor triangle.
		uint16& edgeInactiveTriangle = triAActive ? edge.triangleB : edge.triangleA;
		edgeInactiveTriangle = triangleIndex;
	}

	// Fix up missing indices.
	for (uint32 i = 0; i < numBorderEdges; ++i)
	{
		uint16 edgeIndex = borderEdgeIndices[i];
		epa_edge& edge = edges[edgeIndex];

		bool triangleANew = edge.triangleA >= triangleOffset;
		bool triangleBNew = edge.triangleB >= triangleOffset;
		assert(triangleANew != triangleBNew);

		uint16 pointToConnect = triangleBNew ? edge.a : edge.b; // Other way around than above. This is the point which was connected by another loop iteration above.

		uint16 otherEdgeIndex = newEdgePerPoint[pointToConnect];
		epa_edge& otherEdge = edges[otherEdgeIndex];

		uint16 triangleIndex = i + triangleOffset;
		epa_triangle& tri = triangles[triangleIndex];

		assert(tri.edgeOppositeB == UINT16_MAX);
		assert(otherEdge.triangleA == UINT16_MAX);

		tri.edgeOppositeB = otherEdgeIndex;
		otherEdge.triangleA = triangleIndex;
	}

	return true;
}

