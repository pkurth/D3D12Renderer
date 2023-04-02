#pragma once

#include "core/math.h"
#include "core/hash.h"
#include "geometry/mesh.h"
#include "model_asset.h"

struct full_vertex
{
	vec3 position;
	vec2 uv;
	vec3 normal;
	vec3 tangent;
	uint32 color;
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
			hash_combine(seed, x.tangent);
			hash_combine(seed, x.color);
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








struct per_material
{
	std::unordered_map<full_vertex, uint16> vertexToIndex;
	submesh_asset sub;

	void addTriangles(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals,
		const std::vector<vec3>& tangents, const std::vector<uint32>& colors, const std::vector<skinning_weights>& skins,
		int32 firstIndex, int32 faceSize, std::vector<submesh_asset>& outSubmeshes)
	{
		if (faceSize < 3)
		{
			// Ignore lines and points.
			return;
		}

		int32 aIndex = firstIndex++;
		int32 bIndex = firstIndex++;
		add_vertex_result a = addVertex(positions, uvs, normals, tangents, colors, skins, aIndex);
		add_vertex_result b = addVertex(positions, uvs, normals, tangents, colors, skins, bIndex);
		for (int32 i = 2; i < faceSize; ++i)
		{
			int32 cIndex = firstIndex++;
			add_vertex_result c = addVertex(positions, uvs, normals, tangents, colors, skins, cIndex);

			if (!(a.success && b.success && c.success))
			{
				flush(outSubmeshes);
				a = addVertex(positions, uvs, normals, tangents, colors, skins, aIndex);
				b = addVertex(positions, uvs, normals, tangents, colors, skins, bIndex);
				c = addVertex(positions, uvs, normals, tangents, colors, skins, cIndex);
				printf("Too many vertices for 16-bit indices. Splitting mesh!\n");
			}

			sub.triangles.push_back(indexed_triangle16{ a.index, b.index, c.index });

			b = c;
			bIndex = cIndex;
		}
	}

	void flush(std::vector<submesh_asset>& outSubmeshes)
	{
		if (vertexToIndex.size() > 0)
		{
			outSubmeshes.push_back(std::move(sub));
			vertexToIndex.clear();
		}
	}

private:

	struct add_vertex_result
	{
		uint16 index;
		bool success;
	};

	add_vertex_result addVertex(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals,
		const std::vector<vec3>& tangents, const std::vector<uint32>& colors, const std::vector<skinning_weights>& skins,
		int32 index)
	{
		vec3 position = positions[index];
		vec2 uv = !uvs.empty() ? uvs[index] : vec2(0.f, 0.f);
		vec3 normal = !normals.empty() ? normals[index] : vec3(0.f, 0.f, 0.f);
		vec3 tangent = !tangents.empty() ? tangents[index] : vec3(0.f, 0.f, 0.f);
		uint32 color = !colors.empty() ? colors[index] : 0;
		skinning_weights skin = !skins.empty() ? skins[index] : skinning_weights{};

		full_vertex vertex = { position, uv, normal, tangent, color, skin, };
		auto it = vertexToIndex.find(vertex);
		if (it == vertexToIndex.end())
		{
			uint32 vertexIndex = (uint32)sub.positions.size();
			if (vertexIndex > UINT16_MAX)
			{
				return { 0, false };
			}


			vertexToIndex.insert({ vertex, (uint16)vertexIndex });

			sub.positions.push_back(position);
			if (!uvs.empty()) { sub.uvs.push_back(uv); }
			if (!normals.empty()) { sub.normals.push_back(normal); }
			if (!tangents.empty()) { sub.tangents.push_back(tangent); }
			if (!colors.empty()) { sub.colors.push_back(color); }
			if (!skins.empty()) { sub.skin.push_back(skin); }

			return { (uint16)vertexIndex, true };
		}
		else
		{
			return { it->second, true };
		}
	}
};

void generateNormalsAndTangents(std::vector<submesh_asset>& submeshes, uint32 flags);
