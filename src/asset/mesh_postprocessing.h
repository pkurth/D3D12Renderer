#pragma once

#include "core/math.h"
#include "core/hash.h"
#include "geometry/mesh.h"

struct mesh_geometry
{
	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<int32> indices;
};

struct full_vertex
{
	vec3 position;
	vec2 uv;
	vec3 normal;
	skinning_weights skin;
};

namespace std
{
	template<>
	struct hash<full_vertex>
	{
		size_t operator()(const full_vertex& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.position);
			hash_combine(seed, x.uv);
			hash_combine(seed, x.normal);
			hash_combine(seed, *(uint32*)x.skin.skinIndices);
			hash_combine(seed, *(uint32*)x.skin.skinWeights);

			return seed;
		}
	};
}

static bool operator==(const full_vertex& a, const full_vertex& b)
{
	return memcmp(&a, &b, sizeof(full_vertex)) == 0;
}



mesh_geometry removeDuplicateVertices(mesh_geometry& mesh);
