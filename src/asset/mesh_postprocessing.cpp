#include "pch.h"
#include "mesh_postprocessing.h"

#include <unordered_map>

mesh_geometry removeDuplicateVertices(mesh_geometry& mesh)
{
	std::unordered_map<full_vertex, int32> vertexToIndex;

	vec2 nullUV(0.f, 0.f);
	vec3 nullNormal(0.f, 0.f, 0.f);


	mesh_geometry result;
	result.positions.reserve(mesh.positions.size());
	result.uvs.reserve(mesh.uvs.size());
	result.normals.reserve(mesh.normals.size());
	result.indices = mesh.indices;

	std::vector<int32> uniqueIndexPerVertex(mesh.positions.size());

	for (int32 i = 0; i < (int32)mesh.positions.size(); ++i)
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
