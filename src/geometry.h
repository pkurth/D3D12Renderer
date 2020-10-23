#pragma once

#include "dx_render_primitives.h"
#include "colliders.h"
#include "animation.h"

// Members are always pushed in this order!
enum mesh_creation_flags
{
	mesh_creation_flags_none			= 0,
	mesh_creation_flags_with_positions	= (1 << 0),
	mesh_creation_flags_with_uvs		= (1 << 1),
	mesh_creation_flags_with_normals	= (1 << 2),
	mesh_creation_flags_with_tangents	= (1 << 3),
	mesh_creation_flags_with_skin		= (1 << 4),
};

struct cpu_mesh
{
	cpu_mesh() {}
	cpu_mesh(uint32 flags);
	cpu_mesh(const cpu_mesh& mesh) = delete;
	cpu_mesh(cpu_mesh&& mesh);
	~cpu_mesh();

	uint32 flags;
	uint32 vertexSize;
	uint32 skinOffset;

	uint8* vertices;
	indexed_triangle16* triangles;

	uint32 numVertices;
	uint32 numTriangles;

	submesh_info pushQuad(vec2 radius);
	submesh_info pushQuad(float radius) { return pushQuad(vec2(radius, radius)); }
	submesh_info pushCube(vec3 radius, bool flipWindingOrder = false);
	submesh_info pushCube(float radius, bool flipWindingOrder = false) { return pushCube(vec3(radius, radius, radius), flipWindingOrder); }
	submesh_info pushSphere(uint16 slices, uint16 rows, float radius);
	submesh_info pushCapsule(uint16 slices, uint16 rows, float height, float radius);

	dx_mesh createDXMesh(dx_context* context);
	dx_vertex_buffer createVertexBufferWithAlternativeLayout(dx_context* context, uint32 otherFlags, bool allowUnorderedAccess = false);

private:
	void alignNextTriangle();
	void reserve(uint32 vertexCount, uint32 triangleCount);
	void pushTriangle(uint16 a, uint16 b, uint16 c);
};


