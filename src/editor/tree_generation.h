#pragma once

#include "core/math.h"
#include "geometry/mesh.h"

static float trunkDefaultFunc(float t)
{
	return lerp(1.f, 0.9f, t);
}

struct tree_trunk_generator
{
	float height = 5.f;
	float radiusScale = 1.f;
	catmull_rom_spline<float, 8> radiusFromHeight = trunkDefaultFunc;

	float rootHeight = 1.f;
	float rootNoiseScale = 2.8f;
	float rootNoiseAmplitude = 4.5f;

	uint32 slices = 40;
	uint32 segmentsOverHeight = 8;

	uint32 numVertices = 0;
	uint32 numTriangles = 0;

	uint32 firstVertex = 0;
	uint32 firstTriangle = 0;
};

struct tree_generator
{
	tree_trunk_generator trunk;
	//std::vector<tree_branch_generator> branches;

	std::vector<vec3> positions;
	std::vector<indexed_triangle32> triangles;

	bool edit();

	int32 dirtyFrom = 0;

	dx_mesh generatedMesh;
};
