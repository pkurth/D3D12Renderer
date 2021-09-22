#include "pch.h"
#include "geometry.h"
#include "core/memory.h"

#include <assimp/scene.h>
#include <unordered_map>

struct vertex_info
{
	uint32 othersSize;
	uint32 skinOffset;
};

static vertex_info getVertexInfo(uint32 flags)
{
	vertex_info result = {};
	if (flags & mesh_creation_flags_with_uvs) { result.othersSize += sizeof(vec2); }
	if (flags & mesh_creation_flags_with_normals) { result.othersSize += sizeof(vec3); }
	if (flags & mesh_creation_flags_with_tangents) { result.othersSize += sizeof(vec3); }
	if (flags & mesh_creation_flags_with_skin) { result.skinOffset = result.othersSize;	result.othersSize += sizeof(skinning_weights); }
	return result;
}

cpu_mesh::cpu_mesh(uint32 flags)
{
	this->flags = flags;
	vertex_info info = getVertexInfo(flags);
	othersSize = info.othersSize;
	skinOffset = info.skinOffset;
}

cpu_mesh::~cpu_mesh()
{
	if (vertexPositions)
	{
		_aligned_free(vertexPositions);
	}
	if (vertexOthers)
	{
		_aligned_free(vertexOthers);
	}
	if (triangles)
	{
		_aligned_free(triangles);
	}
}

void cpu_mesh::alignNextTriangle()
{
	// This is called when a new mesh is pushed. The function aligns the next index to a 16-byte boundary.
	numTriangles = alignTo(numTriangles, 8); // 8 triangles are 48 bytes, which is divisible by 16.
}

void cpu_mesh::reserve(uint32 vertexCount, uint32 triangleCount)
{
	vertexPositions = (vec3*)_aligned_realloc(vertexPositions, (numVertices + vertexCount) * sizeof(vec3), 64);
	vertexOthers = (uint8*)_aligned_realloc(vertexOthers, (numVertices + vertexCount) * othersSize, 64);
	triangles = (triangle_t*)_aligned_realloc(triangles, (numTriangles + triangleCount + 8) * sizeof(triangle_t), 64); // Allocate 8 more, such that we can align without problems.
}

#define pushVertex(position, uv, normal, tangent, skin)																							\
	if (flags & mesh_creation_flags_with_positions) { *vertexPositionPtr++ = position; }														\
	if (flags & mesh_creation_flags_with_uvs) { *(vec2*)vertexOthersPtr = uv; vertexOthersPtr += sizeof(vec2); }								\
	if (flags & mesh_creation_flags_with_normals) { *(vec3*)vertexOthersPtr = normal; vertexOthersPtr += sizeof(vec3); }						\
	if (flags & mesh_creation_flags_with_tangents) { *(vec3*)vertexOthersPtr = tangent; vertexOthersPtr += sizeof(vec3); }						\
	if (flags & mesh_creation_flags_with_skin) { *(skinning_weights*)vertexOthersPtr = skin; vertexOthersPtr += sizeof(skinning_weights); }		\
	++this->numVertices;

void cpu_mesh::pushTriangle(index_t a, index_t b, index_t c)
{
	triangles[this->numTriangles++] = { a, b, c };
}

submesh_info cpu_mesh::pushQuad(vec2 radius)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	reserve(4, 2);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;

	pushVertex(vec3(-radius.x, -radius.y, 0.f), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	pushVertex(vec3(radius.x, -radius.y, 0.f), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	pushVertex(vec3(-radius.x, radius.y, 0.f), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	pushVertex(vec3(radius.x, radius.y, 0.f), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});

	pushTriangle(0, 1, 2);
	pushTriangle(1, 3, 2);

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 2 * 3;
	result.baseVertex = baseVertex;
	result.numVertices = 4;
	return result;
}

submesh_info cpu_mesh::pushCube(vec3 radius, bool flipWindingOrder, vec3 center)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	if ((flags & mesh_creation_flags_with_positions)
		&& !(flags & mesh_creation_flags_with_uvs)
		&& !(flags & mesh_creation_flags_with_normals)
		&& !(flags & mesh_creation_flags_with_tangents))
	{
		reserve(8, 12);

		vec3* vertexPositionPtr = vertexPositions + numVertices;
		uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;

		pushVertex(center + vec3(-radius.x, -radius.y, radius.z), {}, {}, {}, {});  // 0
		pushVertex(center + vec3(radius.x, -radius.y, radius.z), {}, {}, {}, {});   // x
		pushVertex(center + vec3(-radius.x, radius.y, radius.z), {}, {}, {}, {});   // y
		pushVertex(center + vec3(radius.x, radius.y, radius.z), {}, {}, {}, {});	// xy
		pushVertex(center + vec3(-radius.x, -radius.y, -radius.z), {}, {}, {}, {}); // z
		pushVertex(center + vec3(radius.x, -radius.y, -radius.z), {}, {}, {}, {});  // xz
		pushVertex(center + vec3(-radius.x, radius.y, -radius.z), {}, {}, {}, {});  // yz
		pushVertex(center + vec3(radius.x, radius.y, -radius.z), {}, {}, {}, {});   // xyz

		pushTriangle(0, 1, 2);
		pushTriangle(1, 3, 2);
		pushTriangle(1, 5, 3);
		pushTriangle(5, 7, 3);
		pushTriangle(5, 4, 7);
		pushTriangle(4, 6, 7);
		pushTriangle(4, 0, 6);
		pushTriangle(0, 2, 6);
		pushTriangle(2, 3, 6);
		pushTriangle(3, 7, 6);
		pushTriangle(4, 5, 0);
		pushTriangle(5, 1, 0);
	}
	else
	{
		reserve(24, 12);

		vec3* vertexPositionPtr = vertexPositions + numVertices;
		uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;

		pushVertex(center + vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(radius.x, radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(center + vec3(radius.x, -radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

		pushTriangle(0, 1, 2);
		pushTriangle(1, 3, 2);
		pushTriangle(4, 5, 6);
		pushTriangle(5, 7, 6);
		pushTriangle(8, 9, 10);
		pushTriangle(9, 11, 10);
		pushTriangle(12, 13, 14);
		pushTriangle(13, 15, 14);
		pushTriangle(16, 17, 18);
		pushTriangle(17, 19, 18);
		pushTriangle(20, 21, 22);
		pushTriangle(21, 23, 22);
	}

	if (flipWindingOrder)
	{
		for (uint32 i = numTriangles - 12; i < numTriangles; ++i)
		{
			index_t tmp = triangles[i].b;
			triangles[i].b = triangles[i].c;
			triangles[i].c = tmp;
		}
	}

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 12 * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushSphere(uint16 slices, uint16 rows, float radius, vec3 center)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);
	assert(rows > 0);

	float vertDeltaAngle = M_PI / (rows + 1);
	float horzDeltaAngle = 2.f * M_PI / slices;

	if (sizeof(index_t) == 2)
	{
		assert(slices * rows + 2 <= UINT16_MAX);
	}
	reserve(slices * rows + 2, 2 * rows * slices);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;

	// Vertices.
	pushVertex(center + vec3(0.f, -radius, 0.f), directionToPanoramaUV(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	for (uint32 y = 0; y < rows; ++y)
	{
		float vertAngle = (y + 1) * vertDeltaAngle - M_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);

			vec2 uv = directionToPanoramaUV(nor);
			pushVertex(center + pos, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}

	pushVertex(center + vec3(0.f, radius, 0.f), directionToPanoramaUV(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	index_t lastVertex = slices * rows + 2;

	// Indices.
	for (index_t x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(0, x + 1u, x + 2u);
	}
	pushTriangle(0, slices, 1);

	for (index_t y = 0; y < rows - 1u; ++y)
	{
		for (index_t x = 0; x < slices - 1u; ++x)
		{
			pushTriangle(y * slices + 1u + x, (y + 1u) * slices + 2u + x, y * slices + 2u + x);
			pushTriangle(y * slices + 1u + x, (y + 1u) * slices + 1u + x, (y + 1u) * slices + 2u + x);
		}
		pushTriangle((index_t)(y * slices + slices), (index_t)((y + 1u) * slices + 1u), (index_t)(y * slices + 1u));
		pushTriangle((index_t)(y * slices + slices), (index_t)((y + 1u) * slices + slices), (index_t)((y + 1u) * slices + 1u));
	}
	for (index_t x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(lastVertex - 2u - x, lastVertex - 3u - x, lastVertex - 1u);
	}
	pushTriangle(lastVertex - 1u - slices, lastVertex - 2u, lastVertex - 1u);

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 2 * rows * slices * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushIcoSphere(float radius, uint32 refinement)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	struct vert
	{
		vec3 p;
		vec3 n;
		vec3 t;
	};

	std::vector<vert> vertices;
	std::vector<triangle_t> triangles;

	float t = (1.f + sqrt(5.f)) / 2.f;

#define push_ico_vertex(p) { vec3 nor = normalize(p); vec3 px = nor * radius; vec3 tan = normalize(cross(vec3(0.f, 1.f, 0.f), nor)); vertices.push_back({px, nor, tan}); }

	push_ico_vertex(vec3(-1.f, t, 0));
	push_ico_vertex(vec3(1.f, t, 0));
	push_ico_vertex(vec3(-1.f, -t, 0));
	push_ico_vertex(vec3(1.f, -t, 0));

	push_ico_vertex(vec3(0, -1.f, t));
	push_ico_vertex(vec3(0, 1.f, t));
	push_ico_vertex(vec3(0, -1.f, -t));
	push_ico_vertex(vec3(0, 1.f, -t));

	push_ico_vertex(vec3(t, 0, -1.f));
	push_ico_vertex(vec3(t, 0, 1.f));
	push_ico_vertex(vec3(-t, 0, -1.f));
	push_ico_vertex(vec3(-t, 0, 1.f));

	triangles.push_back({ 0, 11, 5 });
	triangles.push_back({ 0, 5, 1 });
	triangles.push_back({ 0, 1, 7 });
	triangles.push_back({ 0, 7, 10 });
	triangles.push_back({ 0, 10, 11 });
	triangles.push_back({ 1, 5, 9 });
	triangles.push_back({ 5, 11, 4 });
	triangles.push_back({ 11, 10, 2 });
	triangles.push_back({ 10, 7, 6 });
	triangles.push_back({ 7, 1, 8 });
	triangles.push_back({ 3, 9, 4 });
	triangles.push_back({ 3, 4, 2 });
	triangles.push_back({ 3, 2, 6 });
	triangles.push_back({ 3, 6, 8 });
	triangles.push_back({ 3, 8, 9 });
	triangles.push_back({ 4, 9, 5 });
	triangles.push_back({ 2, 4, 11 });
	triangles.push_back({ 6, 2, 10 });
	triangles.push_back({ 8, 6, 7 });
	triangles.push_back({ 9, 8, 1 });

	std::unordered_map<uint32, index_t> midpoints;

	auto getMiddlePoint = [&midpoints, &vertices, radius](uint32 a, uint32 b)
	{
		uint32 edge = (min(a, b) << 16) | (max(a, b));
		auto it = midpoints.find(edge);
		if (it == midpoints.end())
		{
			vec3 point1 = vertices[a].p;
			vec3 point2 = vertices[b].p;

			vec3 center = 0.5f * (point1 + point2);
			push_ico_vertex(center);

			index_t index = (index_t)vertices.size() - 1;

			midpoints.insert({ edge, index });
			return index;
		}

		return it->second;
	};

	for (uint32 r = 0; r < refinement; ++r)
	{
		std::vector<triangle_t> refinedTriangles;

		for (uint32 tri = 0; tri < (uint32)triangles.size(); ++tri)
		{
			triangle_t& t = triangles[tri];

			index_t a = getMiddlePoint(t.a, t.b);
			index_t b = getMiddlePoint(t.b, t.c);
			index_t c = getMiddlePoint(t.c, t.a);

			refinedTriangles.push_back({ t.a, a, c });
			refinedTriangles.push_back({ t.b, b, a });
			refinedTriangles.push_back({ t.c, c, b });
			refinedTriangles.push_back({ a, b, c });
		}

		triangles = refinedTriangles;
	}

	reserve((uint32)vertices.size(), (uint32)triangles.size());

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;
	for (const vert& v : vertices)
	{
		pushVertex(v.p, {}, v.n, v.t, {});
	}

	for (triangle_t t : triangles)
	{
		pushTriangle(t.a, t.b, t.c);
	}

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = (uint32)triangles.size() * 3;
	result.baseVertex = baseVertex;
	result.numVertices = (uint32)vertices.size();
	return result;

#undef push_ico_vertex
}

submesh_info cpu_mesh::pushCapsule(uint16 slices, uint16 rows, vec3 positionA, vec3 positionB, float radius)
{
	vec3 axis = positionB - positionA;
	float height = length(axis);
	axis /= height;
	vec3 center = (positionA + positionB) * 0.5f;
	return pushCapsule(slices, rows, height, radius, center, axis);
}

submesh_info cpu_mesh::pushCapsule(uint16 slices, uint16 rows, float height, float radius, vec3 center, vec3 upAxis)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);
	assert(rows > 0);
	assert(rows % 2 == 1);

	float vertDeltaAngle = M_PI / (rows + 1);
	float horzDeltaAngle = 2.f * M_PI / slices;
	float halfHeight = 0.5f * height;
	float texStretch = radius / (radius + halfHeight);

	if (sizeof(index_t) == 2)
	{
		assert(slices * (rows + 1) + 2 <= UINT16_MAX);
	}

	reserve(slices * (rows + 1) + 2, 2 * (rows + 1) * slices);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;

	quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), upAxis);

	// Vertices.
	pushVertex(rotation * vec3(0.f, -radius - halfHeight, 0.f) + center, directionToPanoramaUV(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	for (uint32 y = 0; y < rows / 2u + 1u; ++y)
	{
		float vertAngle = (y + 1) * vertDeltaAngle - M_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius - halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);

			vec2 uv = directionToPanoramaUV(nor);
			uv.y *= texStretch;
			pushVertex(rotation * pos + center, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	for (uint32 y = 0; y < rows / 2u + 1u; ++y)
	{
		float vertAngle = (y + rows / 2 + 1) * vertDeltaAngle - M_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius + halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);

			vec2 uv = directionToPanoramaUV(nor);
			uv.y *= texStretch;
			pushVertex(rotation * pos + center, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	pushVertex(rotation * vec3(0.f, radius + halfHeight, 0.f) + center, directionToPanoramaUV(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	index_t lastVertex = slices * (rows + 1) + 2;

	// Indices.
	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(0, x + 1, x + 2);
	}
	pushTriangle(0, slices, 1);
	for (uint32 y = 0; y < rows; ++y)
	{
		for (uint32 x = 0; x < slices - 1u; ++x)
		{
			pushTriangle(y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x);
			pushTriangle(y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x);
		}
		pushTriangle(y * slices + slices, (y + 1) * slices + 1, y * slices + 1);
		pushTriangle(y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1);
	}
	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(lastVertex - 2 - x, lastVertex - 3 - x, lastVertex - 1);
	}
	pushTriangle(lastVertex - 1 - slices, lastVertex - 2, lastVertex - 1);

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 2 * (rows + 1) * slices * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushCylinder(uint16 slices, float radius, float height)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);

	float horzDeltaAngle = 2.f * M_PI / slices;
	float halfHeight = height * 0.5f;

	reserve(4 * slices + 2, 4 * slices);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;
	vec2 uv(0.f, 0.f);
	pushVertex(vec3(0.f, -halfHeight, 0.f), uv, vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	// Bottom row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, -halfHeight, vertexZ * radius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Bottom row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, -halfHeight, vertexZ * radius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, halfHeight, vertexZ * radius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal up.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, halfHeight, vertexZ * radius);
		vec3 nor(0.f, 1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	pushVertex(vec3(0.f, halfHeight, 0.f), uv, vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	index_t lastVertex = 4 * slices + 2;

	// Indices.
	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(0, x + 1, x + 2);
	}
	pushTriangle(0, slices, 1);

	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(slices + 1 + x, 2 * slices + 2 + x, slices + 2 + x);
		pushTriangle(slices + 1 + x, 2 * slices + 1 + x, 2 * slices + 2 + x);
	}
	pushTriangle(slices + slices, 2 * slices + 1, slices + 1);
	pushTriangle(slices + slices, 2 * slices + slices, 2 * slices + 1);

	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(lastVertex - 2 - x, lastVertex - 3 - x, lastVertex - 1);
	}
	pushTriangle(lastVertex - 1 - slices, lastVertex - 2, lastVertex - 1);

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 4 * slices * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushArrow(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);

	float horzDeltaAngle = 2.f * M_PI / slices;

	reserve(7 * slices + 1, 7 * slices);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;
	vec2 uv(0.f, 0.f);
	pushVertex(vec3(0.f, 0.f, 0.f), uv, vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	// Bottom row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Bottom row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top outer row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	vec2 normal2D = normalize(vec2(headLength, headRadius));

	// Top outer row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(vertexX * normal2D.x, normal2D.y, vertexZ * normal2D.x);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top vertex.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(0.f, shaftLength + headLength, 0.f);
		vec3 nor(vertexX * normal2D.x, normal2D.y, vertexZ * normal2D.x);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Indices.
	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(0, x + 1, x + 2);
	}
	pushTriangle(0, slices, 1);

	for (uint32 y = 1; y < 7; y += 2)
	{
		for (uint32 x = 0; x < slices - 1u; ++x)
		{
			pushTriangle(y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x);
			pushTriangle(y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x);
		}
		pushTriangle(y * slices + slices, (y + 1) * slices + 1, y * slices + 1);
		pushTriangle(y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1);
	}

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 7 * slices * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushTorus(uint16 slices, uint16 segments, float torusRadius, float tubeRadius)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);
	assert(segments > 2);

	float tubeDeltaAngle = 2.f * M_PI / slices;
	float torusDeltaAngle = 2.f * M_PI / segments;

	reserve(segments * slices, segments * slices * 2);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;
	vec2 uv(0.f, 0.f);

	quat torusRotation(vec3(1.f, 0.f, 0.f), deg2rad(90.f));

	for (uint32 s = 0; s < segments; ++s)
	{
		float segmentAngle = s * torusDeltaAngle;
		quat segmentRotation(vec3(0.f, 0.f, 1.f), segmentAngle);

		vec3 segmentOffset = segmentRotation * vec3(torusRadius, 0.f, 0.f);

		for (uint32 x = 0; x < slices; ++x)
		{
			float horzAngle = x * tubeDeltaAngle;
			float vertexX = cosf(horzAngle);
			float vertexZ = sinf(horzAngle);
			vec3 pos = torusRotation * (segmentRotation * vec3(vertexX * tubeRadius, 0.f, vertexZ * tubeRadius) + segmentOffset);
			vec3 nor = torusRotation * (segmentRotation * vec3(vertexX, 0.f, vertexZ));

			pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
		}
	}

	index_t numVertices = segments * slices;

	for (uint32 y = 0; y < segments - 1u; ++y)
	{
		for (uint32 x = 0; x < slices - 1u; ++x)
		{
			pushTriangle(y * slices + x, (y + 1) * slices + 1 + x, y * slices + 1 + x);
			pushTriangle(y * slices + x, (y + 1) * slices + x, (y + 1) * slices + 1 + x);
		}
		pushTriangle(y * slices + slices - 1, (y + 1) * slices, y * slices);
		pushTriangle(y * slices + slices - 1, (y + 1) * slices + slices - 1, (y + 1) * slices);
	}

	uint32 y = segments - 1u;
	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(y * slices + x, 1 + x, y * slices + 1 + x);
		pushTriangle(y * slices + x, x, 1 + x);
	}
	pushTriangle(y * slices + slices - 1, 0, y * slices);
	pushTriangle(y * slices + slices - 1, slices - 1, 0);

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = segments * slices * 2 * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushMace(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);

	float horzDeltaAngle = 2.f * M_PI / slices;

	reserve(8 * slices + 2, 8 * slices);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;
	vec2 uv(0.f, 0.f);
	pushVertex(vec3(0.f, 0.f, 0.f), uv, vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	// Bottom row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Bottom row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top outer row, normal down.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(0.f, -1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top outer row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top top outer row, normal around.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength + headLength, vertexZ * headRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top top outer row, normal up.
	for (uint32 x = 0; x < slices; ++x)
	{
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength + headLength, vertexZ * headRadius);
		vec3 nor(0.f, 1.f, 0.f);

		pushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	pushVertex(vec3(0.f, shaftLength + headLength, 0.f), uv, vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	index_t lastVertex = 8 * slices + 2;

	// Indices.
	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(0, x + 1, x + 2);
	}
	pushTriangle(0, slices, 1);

	for (uint32 y = 1; y < 7; y += 2)
	{
		for (uint32 x = 0; x < slices - 1u; ++x)
		{
			pushTriangle(y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x);
			pushTriangle(y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x);
		}
		pushTriangle(y * slices + slices, (y + 1) * slices + 1, y * slices + 1);
		pushTriangle(y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1);
	}

	for (uint32 x = 0; x < slices - 1u; ++x)
	{
		pushTriangle(lastVertex - 2 - x, lastVertex - 3 - x, lastVertex - 1);
	}
	pushTriangle(lastVertex - 1 - slices, lastVertex - 2, lastVertex - 1);

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = 8 * slices * 3;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushAssimpMesh(const aiMesh* mesh, float scale, bounding_box* aabb, animation_skeleton* skeleton)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	if (sizeof(index_t) == 2)
	{
		assert(mesh->mNumVertices <= UINT16_MAX);
	}

	reserve(mesh->mNumVertices, mesh->mNumFaces);

	vec3* vertexPositionPtr = vertexPositions + numVertices;
	uint8* vertexOthersPtr = vertexOthers + othersSize * numVertices;

	vec3 position(0.f, 0.f, 0.f);
	vec3 normal(0.f, 0.f, 0.f);
	vec3 tangent(0.f, 0.f, 0.f);
	vec2 uv(0.f, 0.f);

	bool hasPositions = mesh->HasPositions();
	bool hasNormals = mesh->HasNormals();
	bool hasTangents = mesh->HasTangentsAndBitangents();
	bool hasUVs = mesh->HasTextureCoords(0);

	if (aabb)
	{
		*aabb = bounding_box::negativeInfinity();
	}

	for (uint32 i = 0; i < mesh->mNumVertices; ++i)
	{
		if (hasPositions)
		{
			position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z) * scale;
			if (aabb)
			{
				aabb->grow(position);
			}
		}
		if (hasNormals)
		{
			normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
		if (hasTangents)
		{
			tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
		}
		if (hasUVs)
		{
			uv = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}

		pushVertex(position, uv, normal, tangent, {});
	}

	if ((flags & mesh_creation_flags_with_skin) && skeleton)
	{
		assert(mesh->HasBones());

		uint32 numBones = mesh->mNumBones;

		assert(numBones < 256);

		for (uint32 boneID = 0; boneID < numBones; ++boneID)
		{
			const aiBone* bone = mesh->mBones[boneID];

			auto it = skeleton->nameToJointID.find(bone->mName.C_Str());
			assert(it != skeleton->nameToJointID.end());

			uint8 jointID = (uint8)it->second;

			for (uint32 weightID = 0; weightID < bone->mNumWeights; ++weightID)
			{
				uint32 vertexID = bone->mWeights[weightID].mVertexId;
				float weight = bone->mWeights[weightID].mWeight;

				assert(vertexID < mesh->mNumVertices);
				assert(vertexID + baseVertex < numVertices);

				vertexID += baseVertex;

				uint8* vertexBase = vertexOthers + (vertexID * othersSize);
				skinning_weights& weights = *(skinning_weights*)(vertexBase + skinOffset);

				bool foundFreeSlot = false;
				for (uint32 i = 0; i < 4; ++i)
				{
					if (weights.skinWeights[i] == 0)
					{
						weights.skinIndices[i] = jointID;
						weights.skinWeights[i] = (uint8)clamp(weight * 255.f, 0.f, 255.f);
						foundFreeSlot = true;
						break;
					}
				}
				if (!foundFreeSlot)
				{
					assert(!"Mesh has more than 4 weights per vertex.");
				}
			}
		}

#if 1
		for (uint32 i = 0; i < mesh->mNumVertices; ++i)
		{
			uint8* vertexBase = vertexOthers + ((i + baseVertex) * othersSize);
			skinning_weights& weights = *(skinning_weights*)(vertexBase + skinOffset);

			assert(weights.skinWeights[0] > 0);

			vec4 v = { (float)weights.skinWeights[0], (float)weights.skinWeights[1], (float)weights.skinWeights[2], (float)weights.skinWeights[3] };
			v /= 255.f;

			float sum = dot(v, 1.f);
			if (abs(sum - 1.f) >= 0.05f)
			{
				int a = 0;
			}
		}
#endif
	}


	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		pushTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
	}

	submesh_info result;
	result.firstIndex = firstTriangle * 3;
	result.numIndices = mesh->mNumFaces * 3;
	result.baseVertex = baseVertex;
	result.numVertices = mesh->mNumVertices;
	return result;
}

dx_mesh cpu_mesh::createDXMesh()
{
	dx_mesh result;
	result.vertexBuffer.positions = createVertexBuffer(sizeof(vec3), numVertices, vertexPositions);
	if (flags != mesh_creation_flags_with_positions)
	{
		result.vertexBuffer.others = createVertexBuffer(othersSize, numVertices, vertexOthers);
	}
	result.indexBuffer = createIndexBuffer(sizeof(index_t), numTriangles * 3, triangles);
	return result;
}

#undef pushVertex


