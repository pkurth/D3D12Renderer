#pragma once

#include "math.h"

struct nearest_neighbor_query_result
{
	uint32 index;
	float squaredDistance;
};

struct point_cloud
{
	vec3* positions;
	uint32 numPositions;

	void* index;

	point_cloud(vec3* positions, uint32 numPositions);
	~point_cloud();

	nearest_neighbor_query_result nearestNeighborIndex(vec3 query);





	inline size_t kdtree_get_point_count() const { return numPositions; }
	inline float kdtree_get_pt(const size_t idx, const size_t dim) const { return positions[idx].data[dim]; }
	template <typename BBOX> bool kdtree_get_bbox(BBOX& bb) const { return false; }
};
