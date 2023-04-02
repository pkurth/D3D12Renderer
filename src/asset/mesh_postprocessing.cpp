#include "pch.h"
#include "mesh_postprocessing.h"

#include "core/cpu_profiling.h"

#include <unordered_map>



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


