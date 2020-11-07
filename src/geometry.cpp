#include "pch.h"
#include "geometry.h"
#include "memory.h"

#include <assimp/scene.h>

struct vertex_info
{
	uint32 vertexSize;

	uint32 positionOffset;
	uint32 uvOffset;
	uint32 normalOffset;
	uint32 tangentOffset;
	uint32 skinOffset;
};

static vertex_info getVertexInfo(uint32 flags)
{
	vertex_info result = {};
	if (flags & mesh_creation_flags_with_positions) { result.positionOffset = result.vertexSize;		result.vertexSize += sizeof(vec3); }
	if (flags & mesh_creation_flags_with_uvs) { result.uvOffset = result.vertexSize;			result.vertexSize += sizeof(vec2); }
	if (flags & mesh_creation_flags_with_normals) { result.normalOffset = result.vertexSize;		result.vertexSize += sizeof(vec3); }
	if (flags & mesh_creation_flags_with_tangents) { result.tangentOffset = result.vertexSize;		result.vertexSize += sizeof(vec3); }
	if (flags & mesh_creation_flags_with_skin) { result.skinOffset = result.vertexSize;			result.vertexSize += sizeof(skinning_weights); }
	return result;
}

cpu_mesh::cpu_mesh(uint32 flags)
{
	this->flags = flags;
	vertex_info info = getVertexInfo(flags);
	vertexSize = info.vertexSize;
	skinOffset = info.skinOffset;
}

cpu_mesh::cpu_mesh(cpu_mesh&& mesh)
{
	flags = mesh.flags;
	vertexSize = mesh.vertexSize;
	skinOffset = mesh.skinOffset;
	vertices = mesh.vertices;
	triangles = mesh.triangles;
	numVertices = mesh.numVertices;
	numTriangles = mesh.numTriangles;

	mesh.vertices = 0;
	mesh.triangles = 0;
}

cpu_mesh::~cpu_mesh()
{
	if (vertices)
	{
		_aligned_free(vertices);
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
	vertices = (uint8*)_aligned_realloc(vertices, (numVertices + vertexCount) * vertexSize, 64);
	triangles = (indexed_triangle16*)_aligned_realloc(triangles, (numTriangles + triangleCount + 8) * sizeof(indexed_triangle16), 64); // Allocate 8 more, such that we can align without problems.
}

#define pushVertex(position, uv, normal, tangent, skin)																							\
	if (flags & mesh_creation_flags_with_positions) { *(vec3*)vertexPtr = position; vertexPtr += sizeof(vec3); }								\
	if (flags & mesh_creation_flags_with_uvs) { *(vec2*)vertexPtr = uv; vertexPtr += sizeof(vec2); }											\
	if (flags & mesh_creation_flags_with_normals) { *(vec3*)vertexPtr = normal; vertexPtr += sizeof(vec3); }									\
	if (flags & mesh_creation_flags_with_tangents) { *(vec3*)vertexPtr = tangent; vertexPtr += sizeof(vec3); }									\
	if (flags & mesh_creation_flags_with_skin) { *(skinning_weights*)vertexPtr = skin; vertexPtr += sizeof(skinning_weights); }		\
	++this->numVertices;

void cpu_mesh::pushTriangle(uint16 a, uint16 b, uint16 c)
{
	triangles[this->numTriangles++] = { a, b, c };
}

submesh_info cpu_mesh::pushQuad(vec2 radius)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	reserve(4, 2);

	uint8* vertexPtr = vertices + vertexSize * numVertices;

	pushVertex(vec3(-radius.x, -radius.y, 0.f), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	pushVertex(vec3(radius.x, -radius.y, 0.f), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	pushVertex(vec3(-radius.x, radius.y, 0.f), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	pushVertex(vec3(radius.x, radius.y, 0.f), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});

	pushTriangle(0, 1, 2);
	pushTriangle(1, 3, 2);

	submesh_info result;
	result.firstTriangle = firstTriangle;
	result.numTriangles = 2;
	result.baseVertex = baseVertex;
	result.numVertices = 4;
	return result;
}

submesh_info cpu_mesh::pushCube(vec3 radius, bool flipWindingOrder)
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

		uint8* vertexPtr = vertices + vertexSize * numVertices;

		pushVertex(vec3(-radius.x, -radius.y, radius.z), {}, {}, {}, {});  // 0
		pushVertex(vec3(radius.x, -radius.y, radius.z), {}, {}, {}, {});   // x
		pushVertex(vec3(-radius.x, radius.y, radius.z), {}, {}, {}, {});   // y
		pushVertex(vec3(radius.x, radius.y, radius.z), {}, {}, {}, {});	 // xy
		pushVertex(vec3(-radius.x, -radius.y, -radius.z), {}, {}, {}, {}); // z
		pushVertex(vec3(radius.x, -radius.y, -radius.z), {}, {}, {}, {});  // xz
		pushVertex(vec3(-radius.x, radius.y, -radius.z), {}, {}, {}, {});  // yz
		pushVertex(vec3(radius.x, radius.y, -radius.z), {}, {}, {}, {});   // xyz

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

		uint8* vertexPtr = vertices + vertexSize * numVertices;

		pushVertex(vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		pushVertex(vec3(-radius.x, radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(radius.x, radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		pushVertex(vec3(radius.x, -radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

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
			uint16 tmp = triangles[i].b;
			triangles[i].b = triangles[i].c;
			triangles[i].c = tmp;
		}
	}

	submesh_info result;
	result.firstTriangle = firstTriangle;
	result.numTriangles = 12;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushSphere(uint16 slices, uint16 rows, float radius)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(slices > 2);
	assert(rows > 0);

	float vertDeltaAngle = M_PI / (rows + 1);
	float horzDeltaAngle = 2.f * M_PI / slices;

	assert(slices * rows + 2 <= UINT16_MAX);

	reserve(slices * rows + 2, 2 * rows * slices);

	uint8* vertexPtr = vertices + vertexSize * numVertices;

	// Vertices.
	pushVertex(vec3(0.f, -radius, 0.f), directionToPanoramaUV(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

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
			pushVertex(pos, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}

	pushVertex(vec3(0.f, radius, 0.f), directionToPanoramaUV(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	uint16 lastVertex = slices * rows + 2;

	// Indices.
	for (uint16 x = 0; x < slices - 1; ++x)
	{
		pushTriangle(0, x + 1u, x + 2u);
	}
	pushTriangle(0, slices, 1);

	for (uint16 y = 0; y < rows - 1; ++y)
	{
		for (uint16 x = 0; x < slices - 1; ++x)
		{
			pushTriangle(y * slices + 1u + x, (y + 1u) * slices + 2u + x, y * slices + 2u + x);
			pushTriangle(y * slices + 1u + x, (y + 1u) * slices + 1u + x, (y + 1u) * slices + 2u + x);
		}
		pushTriangle((uint16)(y * slices + slices), (uint16)((y + 1u) * slices + 1u), (uint16)(y * slices + 1u));
		pushTriangle((uint16)(y * slices + slices), (uint16)((y + 1u) * slices + slices), (uint16)((y + 1u) * slices + 1u));
	}
	for (uint16 x = 0; x < slices - 1; ++x)
	{
		pushTriangle(lastVertex - 2u - x, lastVertex - 3u - x, lastVertex - 1u);
	}
	pushTriangle(lastVertex - 1u - slices, lastVertex - 2u, lastVertex - 1u);

	submesh_info result;
	result.firstTriangle = firstTriangle;
	result.numTriangles = 2 * rows * slices;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushCapsule(uint16 slices, uint16 rows, float height, float radius)
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

	assert(slices * (rows + 1) + 2 <= UINT16_MAX);

	reserve(slices * (rows + 1) + 2, 2 * (rows + 1) * slices);

	uint8* vertexPtr = vertices + vertexSize * numVertices;

	// Vertices.
	pushVertex(vec3(0.f, -radius - halfHeight, 0.f), directionToPanoramaUV(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

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
			pushVertex(pos, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
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
			pushVertex(pos, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	pushVertex(vec3(0.f, radius + halfHeight, 0.f), directionToPanoramaUV(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	uint16 lastVertex = slices * (rows + 1) + 2;

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
	result.firstTriangle = firstTriangle;
	result.numTriangles = 2 * (rows + 1) * slices;
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

	uint8* vertexPtr = vertices + vertexSize * numVertices;
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

	uint16 lastVertex = 4 * slices + 2;

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
	result.firstTriangle = firstTriangle;
	result.numTriangles = 4 * slices;
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

	uint8* vertexPtr = vertices + vertexSize * numVertices;
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
	result.firstTriangle = firstTriangle;
	result.numTriangles = 7 * slices;
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

	uint8* vertexPtr = vertices + vertexSize * numVertices;
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

	uint16 numVertices = segments * slices;

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
	result.firstTriangle = firstTriangle;
	result.numTriangles = segments * slices * 2;
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

	uint8* vertexPtr = vertices + vertexSize * numVertices;
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

	uint16 lastVertex = 8 * slices + 2;

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
	result.firstTriangle = firstTriangle;
	result.numTriangles = 8 * slices;
	result.baseVertex = baseVertex;
	result.numVertices = numVertices - baseVertex;
	return result;
}

submesh_info cpu_mesh::pushAssimpMesh(const aiMesh* mesh, float scale, aabb_collider* aabb)
{
	alignNextTriangle();

	uint32 baseVertex = numVertices;
	uint32 firstTriangle = numTriangles;

	assert(mesh->mNumVertices <= UINT16_MAX);

	reserve(mesh->mNumVertices, mesh->mNumFaces);

	uint8* vertexPtr = vertices + vertexSize * numVertices;

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
		*aabb = aabb_collider::negativeInfinity();
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

#if 0
	if ((flags & mesh_creation_flags_with_skin) && skeleton)
	{
		assert(mesh->HasBones());

		uint32 numBones = mesh->mNumBones;
		assert(numBones < MAX_NUM_JOINTS);

		for (uint32 boneID = 0; boneID < numBones; ++boneID)
		{
			const aiBone* bone = mesh->mBones[boneID];
			uint32 jointID = -1;
			for (uint32 i = 0; i < skeleton->numJoints; ++i)
			{
				skeleton_joint& j = skeleton->joints[i];
				if (stringsAreEqual(j.name, bone->mName.C_Str()))
				{
					jointID = i;
					break;
				}
			}
			assert(jointID != -1);

			for (uint32 weightID = 0; weightID < bone->mNumWeights; ++weightID)
			{
				uint32 vertexID = bone->mWeights[weightID].mVertexId;
				float weight = bone->mWeights[weightID].mWeight;

				assert(vertexID + baseVertex < numVertices);

				vertexID += baseVertex;
				uint8* vertexBase = vertices + (vertexID * vertexSize);
				skinning_weights& weights = *(skinning_weights*)(vertexBase + skinOffset);

				for (uint32 i = 0; i < 4; ++i)
				{
					if (weights.skinWeights[i] == 0)
					{
						weights.skinIndices[i] = (uint8)jointID;
						weights.skinWeights[i] = (uint8)clamp(weight * 255.f, 0.f, 255.f);
						break;
					}
				}
			}
		}
	}
#endif


	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		pushTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
	}

	submesh_info result;
	result.firstTriangle = firstTriangle;
	result.numTriangles = mesh->mNumFaces;
	result.baseVertex = baseVertex;
	result.numVertices = mesh->mNumVertices;
	return result;
}

dx_mesh cpu_mesh::createDXMesh()
{
	dx_mesh result;
	result.vertexBuffer = createVertexBuffer(vertexSize, numVertices, vertices);
	result.indexBuffer = createIndexBuffer(sizeof(uint16), numTriangles * 3, triangles);
	return result;
}

#define getVertexProperty(prop, base, info, type) *(type*)(base + info.prop##Offset) 

dx_vertex_buffer cpu_mesh::createVertexBufferWithAlternativeLayout(uint32 otherFlags, bool allowUnorderedAccess)
{
#ifdef _DEBUG
	for (uint32 i = 0; i < 31; ++i)
	{
		uint32 testFlag = (1 << i);
		if (otherFlags & testFlag)
		{
			assert(flags & testFlag); // We can only remove flags, not set new flags.
		}
	}
#endif

	vertex_info ownInfo = getVertexInfo(flags);
	vertex_info newInfo = getVertexInfo(otherFlags);

	uint8* newVertices = (uint8*)malloc(newInfo.vertexSize * numVertices);

	for (uint32 i = 0; i < numVertices; ++i)
	{
		uint8* ownBase = vertices + i * ownInfo.vertexSize;
		uint8* newBase = newVertices + i * newInfo.vertexSize;

		if (otherFlags & mesh_creation_flags_with_positions) { getVertexProperty(position, newBase, newInfo, vec3) = getVertexProperty(position, ownBase, ownInfo, vec3); }
		if (otherFlags & mesh_creation_flags_with_uvs) { getVertexProperty(uv, newBase, newInfo, vec2) = getVertexProperty(uv, ownBase, ownInfo, vec2); }
		if (otherFlags & mesh_creation_flags_with_normals) { getVertexProperty(normal, newBase, newInfo, vec3) = getVertexProperty(normal, ownBase, ownInfo, vec3); }
		if (otherFlags & mesh_creation_flags_with_tangents) { getVertexProperty(tangent, newBase, newInfo, vec3) = getVertexProperty(tangent, ownBase, ownInfo, vec3); }
		if (otherFlags & mesh_creation_flags_with_skin) { getVertexProperty(skin, newBase, newInfo, skinning_weights) = getVertexProperty(skin, ownBase, ownInfo, skinning_weights); }
	}

	dx_vertex_buffer vertexBuffer = createVertexBuffer(newInfo.vertexSize, numVertices, newVertices, allowUnorderedAccess);
	free(newVertices);
	return vertexBuffer;
}

#undef getVertexProperty
#undef pushVertex


