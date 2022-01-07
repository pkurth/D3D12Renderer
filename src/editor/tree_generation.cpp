#include "pch.h"
#include "tree_generation.h"
#include "core/imgui.h"
#include "core/perlin.h"



static void generateTrunk(tree_trunk_generator& gen, std::vector<vec3>& positions, std::vector<indexed_triangle32>& triangles)
{
	gen.firstVertex = (uint32)positions.size();
	gen.firstTriangle = (uint32)triangles.size();

	struct sincos
	{
		float sin, cos;
	};

	uint32 slices = gen.slices;
	uint32 segments = gen.segmentsOverHeight;
	float height = gen.height;
	vec3 base = 0.f;

	assert(slices > 2);
	assert(segments > 1);

	float horzDeltaAngle = 2.f * M_PI / slices;
	float halfHeight = height * 0.5f;

	sincos* angles = (sincos*)alloca(sizeof(sincos) * slices);
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		angles[x] = { sinf(horzAngle), cosf(horzAngle) };
	}

	for (uint32 s = 0; s < segments; ++s)
	{
		float t = (float)s / (segments - 1);
		float radius = gen.radiusFromHeight.evaluate(gen.radiusFromHeight.maxNumPoints, t) * gen.radiusScale;

		float h = t * height;

		for (uint32 x = 0; x < slices; ++x)
		{
			float o = 0.f;
			if (h <= gen.rootHeight)
			{
				o = lerp((perlinNoise(x * gen.rootNoiseScale) - 0.5f) * gen.rootNoiseAmplitude, 0.f, smoothstep(h / gen.rootHeight));
			}
			float r = radius + o;

			float vertexX = angles[x].cos;
			float vertexZ = angles[x].sin;

			vec3 pos(vertexX * r, h, vertexZ * r);
			positions.push_back(pos + base);
		}
	}

	//positions.push_back(rotation * vec3(0.f, height, 0.f));

	for (uint32 s = 0; s < segments - 1; ++s)
	{
		for (uint32 x = 0; x < slices; ++x)
		{
			uint32 x1 = (x + 1) % slices;

			uint32 bl = gen.firstVertex + s * slices + x;
			uint32 br = gen.firstVertex + s * slices + x1;
			uint32 tl = gen.firstVertex + (s + 1) * slices + x;
			uint32 tr = gen.firstVertex + (s + 1) * slices + x1;

			triangles.push_back({ bl, tl, br });
			triangles.push_back({ br, tl, tr });
		}
	}

	gen.numVertices = (uint32)positions.size() - gen.firstVertex;
	gen.numTriangles = (uint32)triangles.size() - gen.firstTriangle;
}

bool tree_generator::edit()
{
	if (ImGui::BeginTree("Trunk"))
	{
		bool change = false;

		if (ImGui::BeginProperties())
		{
			change |= ImGui::PropertyDrag("Height", trunk.height, 0.05f);
			change |= ImGui::PropertyDrag("Root height", trunk.rootHeight, 0.05f);
			change |= ImGui::PropertyDrag("Root noise amplitude", trunk.rootNoiseAmplitude, 0.05f);
			change |= ImGui::PropertyDrag("Root noise scale", trunk.rootNoiseScale, 0.05f);
			change |= ImGui::PropertyDrag("Slices", trunk.slices);
			change |= ImGui::PropertyDrag("Segments over height", trunk.segmentsOverHeight);

			ImGui::EndProperties();
		}
		change |= ImGui::Spline("Radius from height", ImVec2(0, 0), trunk.radiusFromHeight);


		if (change)
		{
			dirtyFrom = min(dirtyFrom, 0);
		}
		
		ImGui::EndTree();
	}



	bool result = dirtyFrom != INT32_MAX;

	if (result)
	{
		// TODO: Generate from first dirty part on.
		positions.resize(trunk.firstVertex);
		triangles.resize(trunk.firstTriangle);
		generateTrunk(trunk, positions, triangles);


		generatedMesh.vertexBuffer.positions = createVertexBuffer(sizeof(vec3), (uint32)positions.size(), positions.data());
		generatedMesh.indexBuffer = createIndexBuffer(sizeof(uint32), (uint32)triangles.size() * 3, triangles.data());
	}

	dirtyFrom = INT32_MAX;


	return result;
}
