#include "pch.h"
#include "mesh_postprocessing.h"

#include "core/hash.h"

#include <unordered_map>

struct full_vertex
{
	vec3 position;
	vec2 uv;
	vec3 normal;
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

			return seed;
		}
	};
}

static bool operator==(const full_vertex& a, const full_vertex& b)
{
	return memcmp(&a, &b, sizeof(full_vertex)) == 0;
}

mesh_geometry removeDuplicateVertices(mesh_geometry& mesh)
{
	std::unordered_map<full_vertex, uint32> vertexToIndex;

	vec2 nullUV(0.f, 0.f);
	vec3 nullNormal(0.f, 0.f, 0.f);


	mesh_geometry result;
	result.positions.reserve(mesh.positions.size());
	result.uvs.reserve(mesh.uvs.size());
	result.normals.reserve(mesh.normals.size());
	result.indices = mesh.indices;

	std::vector<int32> uniqueIndexPerVertex(mesh.positions.size());

	for (uint32 i = 0; i < (uint32)mesh.positions.size(); ++i)
	{
		full_vertex vertex = 
		{ 
			mesh.positions[i],
			!mesh.uvs.empty() ? mesh.uvs[i] : nullUV,
			!mesh.normals.empty() ? mesh.normals[i] : nullNormal,
		};

		auto it = vertexToIndex.find(vertex);
		if (it == vertexToIndex.end())
		{
			int32 vertexIndex = (int32)result.positions.size();
			vertexToIndex.insert({ vertex, vertexIndex });
			uniqueIndexPerVertex[i] = vertexIndex;

			result.positions.push_back(vertex.position);
			if (!mesh.uvs.empty()) { result.uvs.push_back(vertex.uv); }
			if (!mesh.normals.empty()) { result.normals.push_back(vertex.normal); }
		}
		else
		{
			uniqueIndexPerVertex[i] = it->second;
		}
	}

	for (int32& i : result.indices)
	{
		i = uniqueIndexPerVertex[i];
	}

	return result;
}
