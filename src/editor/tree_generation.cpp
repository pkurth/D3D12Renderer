#include "pch.h"
#include "tree_generation.h"
#include "core/imgui.h"
#include "core/perlin.h"
#include "core/random.h"


struct sincos
{
	float sin, cos, angle;
};

static void generateAngles(sincos* angles, uint32 slices)
{
	float horzDeltaAngle = 2.f * M_PI / slices;

	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		angles[x] = { sinf(horzAngle), cosf(horzAngle), horzAngle };
	}
}

static vec3 getTrunkPoint(const tree_trunk_generator& gen, sincos angles, float radius, float height)
{
	float o = 0.f;
	if (height <= gen.rootHeight)
	{
		o = lerp((perlinNoise(angles.angle * gen.rootNoiseScale) - 0.5f) * gen.rootNoiseAmplitude, 0.f, smoothstep(height / gen.rootHeight));
	}
	float r = radius + o;

	float vertexX = angles.cos;
	float vertexZ = angles.sin;

	return vec3(vertexX * r, height, vertexZ * r);
}

static void generateCylinderTriangles(uint32 segments, uint32 slices, uint32 firstVertex, std::vector<indexed_triangle32>& triangles)
{
	for (uint32 s = 0; s < segments - 1; ++s)
	{
		for (uint32 x = 0; x < slices; ++x)
		{
			uint32 x1 = (x + 1) % slices;

			uint32 bl = firstVertex + s * slices + x;
			uint32 br = firstVertex + s * slices + x1;
			uint32 tl = firstVertex + (s + 1) * slices + x;
			uint32 tr = firstVertex + (s + 1) * slices + x1;

			triangles.push_back({ bl, tl, br });
			triangles.push_back({ br, tl, tr });
		}
	}
}

static void generateTrunk(tree_trunk_generator& gen, std::vector<vec3>& positions, std::vector<vertex_uv_normal_tangent>& others, 
	std::vector<indexed_triangle32>& triangles)
{
	gen.firstVertex = (uint32)positions.size();
	gen.firstTriangle = (uint32)triangles.size();

	uint32 slices = gen.slices;
	uint32 segments = gen.segmentsOverHeight;
	float height = gen.height;

	assert(slices > 2);
	assert(segments > 1);

	sincos* angles = (sincos*)alloca(sizeof(sincos) * slices);
	generateAngles(angles, slices);

	for (uint32 s = 0; s < segments; ++s)
	{
		float t = (float)s / (segments - 1);
		float radius = gen.radiusFromHeight.evaluate(gen.radiusFromHeight.maxNumPoints, t) * gen.radiusScale;

		float h = t * height;

		for (uint32 x = 0; x < slices; ++x)
		{
			vec3 pos = getTrunkPoint(gen, angles[x], radius, h);
			positions.push_back(pos);
			others.push_back({ vec2((float)x / (slices - 1), t), vec3(0.f) });
		}
	}

	generateCylinderTriangles(segments, slices, gen.firstVertex, triangles);

	gen.numVertices = (uint32)positions.size() - gen.firstVertex;
	gen.numTriangles = (uint32)triangles.size() - gen.firstTriangle;
}

static void generateBranches(const tree_trunk_generator& parent, tree_branch_generator& gen,
	std::vector<vec3>& positions, std::vector<vertex_uv_normal_tangent>& others,
	std::vector<indexed_triangle32>& triangles, random_number_generator& rng)
{
	gen.firstVertex = (uint32)positions.size();
	gen.firstTriangle = (uint32)triangles.size();

	uint32 slices = gen.slices;
	uint32 segments = gen.segmentsOverLength;
	float baseLength = gen.baseLength;

	assert(slices > 2);
	assert(segments > 1);

	sincos* angles = (sincos*)alloca(sizeof(sincos) * slices);
	generateAngles(angles, slices);

	for (uint32 b = 0; b < gen.numBranches; ++b)
	{
		uint32 firstVertex = (uint32)positions.size();

		float parentT = rng.randomFloatBetween(gen.parentRangeFrom, gen.parentRangeTo);

		float parentHorzAngle = rng.randomFloatBetween(0.f, M_TAU);
		sincos parentAngles = { sin(parentHorzAngle), cos(parentHorzAngle), parentHorzAngle };

		float parentRadius = parent.radiusFromHeight.evaluate(parent.radiusFromHeight.maxNumPoints, parentT) * parent.radiusScale;
		float parentH = parentT * parent.height;
		
		vec3 base = getTrunkPoint(parent, parentAngles, parentRadius, parentH);
		quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), normalize(vec3(base.x, 1.f, base.z)));

		vec3 segmentCenter = base;
		float segmentStride = 1.f / (segments - 1);

		for (uint32 s = 0; s < segments; ++s)
		{
			float t = (float)s / (segments - 1);
			float radius = gen.radiusFromLength.evaluate(gen.radiusFromLength.maxNumPoints, t) * gen.radiusScale;

			float l = t * baseLength;

			for (uint32 x = 0; x < slices; ++x)
			{
				float vertexX = angles[x].cos;
				float vertexZ = angles[x].sin;

				vec3 pos = segmentCenter + rotation * vec3(vertexX, 0.f, vertexZ) * radius;

				positions.push_back(pos);
				others.push_back({ vec2((float)x / (slices - 1), t), vec3(0.f) });
			}

			segmentCenter += rotation * vec3(0.f, segmentStride, 0.f);
			rotation = slerp(rotation, quat::identity, 0.05f);
		}

		generateCylinderTriangles(segments, slices, firstVertex, triangles);
	}


	gen.numVertices = (uint32)positions.size() - gen.firstVertex;
	gen.numTriangles = (uint32)triangles.size() - gen.firstTriangle;
}

static void generateOthers(const std::vector<vec3>& positions, std::vector<vertex_uv_normal_tangent>& others, 
	const std::vector<indexed_triangle32>& triangles)
{
	for (const auto& tri : triangles)
	{
		vec3 a = positions[tri.a];
		vec3 b = positions[tri.b];
		vec3 c = positions[tri.c];

		vec3 n = cross(b - a, c - a);

		others[tri.a].normal += n;
		others[tri.b].normal += n;
		others[tri.c].normal += n;
	}

	for (auto& o : others)
	{
		o.normal = normalize(o.normal);
	}
}

bool tree_generator::edit()
{
	if (ImGui::BeginProperties())
	{
		bool change = false;

		change |= ImGui::PropertyInput("Seed", seed);
		ImGui::EndProperties();

		if (change)
		{
			dirtyFrom = 0;
		}
	}

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

	if (ImGui::BeginTree("Branches"))
	{
		bool change = false;

		if (ImGui::BeginProperties())
		{
			change |= ImGui::PropertyDrag("Num branches", branches.numBranches);
			change |= ImGui::PropertyDrag("Base length", branches.baseLength, 0.05f);
			//change |= ImGui::PropertyDrag("Root height", trunk.rootHeight, 0.05f);
			//change |= ImGui::PropertyDrag("Root noise amplitude", trunk.rootNoiseAmplitude, 0.05f);
			//change |= ImGui::PropertyDrag("Root noise scale", trunk.rootNoiseScale, 0.05f);
			//change |= ImGui::PropertyDrag("Slices", trunk.slices);
			//change |= ImGui::PropertyDrag("Segments over height", trunk.segmentsOverHeight);

			ImGui::EndProperties();
		}
		//change |= ImGui::Spline("Radius from length", ImVec2(0, 0), branches.radiusFromLength);

		if (change)
		{
			dirtyFrom = min(dirtyFrom, 1);
		}

		ImGui::EndTree();
	}



	bool result = dirtyFrom != INT32_MAX;

	if (result)
	{
		random_number_generator rng = seed;

		if (dirtyFrom == 0)
		{
			positions.resize(trunk.firstVertex);
			others.resize(trunk.firstVertex);
			triangles.resize(trunk.firstTriangle);
		}
		if (dirtyFrom == 1)
		{
			positions.resize(branches.firstVertex);
			others.resize(branches.firstVertex);
			triangles.resize(branches.firstTriangle);
		}

		if (dirtyFrom <= 0)
		{
			generateTrunk(trunk, positions, others, triangles);
		}
		if (dirtyFrom <= 1)
		{
			generateBranches(trunk, branches, positions, others, triangles, rng);
		}

		generateOthers(positions, others, triangles);

		generatedMesh.vertexBuffer.positions = createVertexBuffer(sizeof(vec3), (uint32)positions.size(), positions.data());
		generatedMesh.vertexBuffer.others = createVertexBuffer(sizeof(vertex_uv_normal_tangent), (uint32)others.size(), others.data());
		generatedMesh.indexBuffer = createIndexBuffer(sizeof(uint32), (uint32)triangles.size() * 3, triangles.data());
	}

	dirtyFrom = INT32_MAX;


	return result;
}
