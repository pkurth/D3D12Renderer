#pragma once

#include "core/math.h"

struct mesh_geometry
{
	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<int32> indices;
};

mesh_geometry removeDuplicateVertices(mesh_geometry& mesh);
