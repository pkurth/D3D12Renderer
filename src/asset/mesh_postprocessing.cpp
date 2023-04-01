#include "pch.h"
#include "mesh_postprocessing.h"
#include "model_asset.h"

#include "core/cpu_profiling.h"

#include <unordered_map>


void triangulateAndRemoveDuplicateVertices(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals,
	const std::vector<vec3>& tangents, const std::vector<skinning_weights>& skins, const std::vector<int32>& originalIndices,
	const std::vector<int32>& materialIndexPerFace,
	std::vector<submesh_asset>& outSubmeshes)
{

	struct per_material
	{
		std::unordered_map<full_vertex, uint16> vertexToIndex;
		submesh_asset sub;

		void addTriangles(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals,
			const std::vector<vec3>& tangents, const std::vector<skinning_weights>& skins,
			int32 firstIndex, int32 faceSize, std::vector<submesh_asset>& outSubmeshes)
		{
			if (faceSize < 3)
			{
				// Ignore lines and points.
				return;
			}

			int32 aIndex = firstIndex++;
			int32 bIndex = firstIndex++;
			add_vertex_result a = addVertex(positions, uvs, normals, tangents, skins, aIndex);
			add_vertex_result b = addVertex(positions, uvs, normals, tangents, skins, bIndex);
			for (int32 i = 2; i < faceSize; ++i)
			{
				int32 cIndex = firstIndex++;
				add_vertex_result c = addVertex(positions, uvs, normals, tangents, skins, cIndex);

				if (!(a.success && b.success && c.success))
				{
					flush(outSubmeshes);
					a = addVertex(positions, uvs, normals, tangents, skins, aIndex);
					b = addVertex(positions, uvs, normals, tangents, skins, bIndex);
					c = addVertex(positions, uvs, normals, tangents, skins, cIndex);
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
			const std::vector<vec3>& tangents, const std::vector<skinning_weights>& skins,
			int32 index)
		{
			vec3 position = positions[index];
			vec2 uv = !uvs.empty() ? uvs[index] : vec2(0.f, 0.f);
			vec3 normal = !normals.empty() ? normals[index] : vec3(0.f, 0.f, 0.f);
			vec3 tangent = !tangents.empty() ? tangents[index] : vec3(0.f, 0.f, 0.f);
			skinning_weights skin = !skins.empty() ? skins[index] : skinning_weights{};

			full_vertex vertex = { position, uv, normal, tangent, skin, };
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
				if (!skins.empty()) { sub.skin.push_back(skin); }

				return { (uint16)vertexIndex, true };
			}
			else
			{
				return { it->second, true };
			}
		}
	};

	std::unordered_map<int32, per_material> materialToMesh;

	{
		CPU_PRINT_PROFILE_BLOCK("Triangulating & duplicate vertex removal");
		int32 faceIndex = 0;
		for (int32 firstIndex = 0, end = (int32)originalIndices.size(); firstIndex < end;)
		{
			int32 faceSize = 0;
			while (firstIndex + faceSize < end)
			{
				int32 i = firstIndex + faceSize++;
				if (originalIndices[i] < 0)
				{
					break;
				}
			}

			int32 material = materialIndexPerFace[faceIndex++];

			per_material& perMat = materialToMesh[material];
			perMat.sub.materialIndex = material;

			perMat.addTriangles(positions, uvs, normals, tangents, skins, firstIndex, faceSize, outSubmeshes);

			firstIndex += faceSize;
		}
	}

	for (auto [i, perMat] : materialToMesh)
	{
		perMat.flush(outSubmeshes);
	}
}

void generateNormalsAndTangents(std::vector<submesh_asset>& submeshes, uint32 flags)
{
	if (flags & mesh_flag_gen_tangents)
	{
		flags |= mesh_flag_gen_normals;
	}

	for (submesh_asset& sub : submeshes)
	{
		if (sub.normals.empty() && flags & mesh_flag_gen_normals)
		{
			printf("Generating normals\n");
			CPU_PRINT_PROFILE_BLOCK("Generating normals");

			sub.normals.resize(sub.positions.size(), vec3(0.f));
			for (indexed_triangle16 tri : sub.triangles)
			{
				vec3 a = sub.positions[tri.a];
				vec3 b = sub.positions[tri.b];
				vec3 c = sub.positions[tri.c];

				vec3 n = cross(b - a, c - a);
				sub.normals[tri.a] += n;
				sub.normals[tri.b] += n;
				sub.normals[tri.c] += n;
			}
			for (vec3& n : sub.normals)
			{
				n = normalize(n);
			}
		}

		if (sub.tangents.empty() && flags & mesh_flag_gen_tangents)
		{
			printf("Generating tangents\n");
			CPU_PRINT_PROFILE_BLOCK("Generating tangents");

			sub.tangents.resize(sub.positions.size(), vec3(0.f));
			if (!sub.uvs.empty())
			{
				// https://stackoverflow.com/a/5257471
				for (indexed_triangle16 tri : sub.triangles)
				{
					vec3 a = sub.positions[tri.a];
					vec3 b = sub.positions[tri.b];
					vec3 c = sub.positions[tri.c];

					vec2 h = sub.uvs[tri.a];
					vec2 k = sub.uvs[tri.b];
					vec2 l = sub.uvs[tri.c];

					vec3 d = b - a;
					vec3 e = c - a;

					vec2 f = k - h;
					vec2 g = l - h;

					float invDet = 1.f / (f.x * g.y - f.y * g.x);

					vec3 t;
					t.x = g.y * d.x - f.y * e.x;
					t.y = g.y * d.y - f.y * e.y;
					t.z = g.y * d.z - f.y * e.z;
					t *= invDet;
					sub.tangents[tri.a] += t;
					sub.tangents[tri.b] += t;
					sub.tangents[tri.c] += t;
				}
				for (uint32 i = 0; i < (uint32)sub.positions.size(); ++i)
				{
					vec3 t = sub.tangents[i];
					vec3 n = sub.normals[i];

					// Make sure tangent is perpendicular to normal.
					vec3 b = cross(t, n);
					t = cross(n, b);

					sub.tangents[i] = normalize(t);
				}
			}
			else
			{
				printf("Mesh has no UVs. Generating suboptimal tangents.\n");
				for (uint32 i = 0; i < (uint32)sub.positions.size(); ++i)
				{
					sub.tangents[i] = getTangent(sub.normals[i]);
				}
			}
		}
	}
}


