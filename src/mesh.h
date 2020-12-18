#pragma once

#include "bounding_volumes.h"
#include "dx_buffer.h"
#include "animation.h"

struct pbr_material;


struct submesh_info
{
	uint32 numTriangles;
	uint32 firstTriangle;
	uint32 baseVertex;
	uint32 numVertices;
};

struct submesh
{
	submesh_info info;
	bounding_box aabb; // In composite's local space.
	trs transform;

	ref<pbr_material> material;
	std::string name;
};

struct composite_mesh
{
	std::vector<submesh> submeshes;
	animation_skeleton skeleton;
	dx_mesh mesh;
};


composite_mesh loadMeshFromFile(const char* sceneFilename, uint32 flags);
