#pragma once

#include "physics/bounding_volumes.h"
#include "animation/animation.h"
#include "dx/dx_buffer.h"



struct submesh_info
{
	uint32 numIndices;
	uint32 firstIndex;
	uint32 baseVertex;
	uint32 numVertices;
};


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

static uint32 getVertexSize(uint32 meshFlags)
{
	uint32 size = 0;
	//if (meshFlags & mesh_creation_flags_with_positions) { size += sizeof(vec3); }
	if (meshFlags & mesh_creation_flags_with_uvs) { size += sizeof(vec2); }
	if (meshFlags & mesh_creation_flags_with_normals) { size += sizeof(vec3); }
	if (meshFlags & mesh_creation_flags_with_tangents) { size += sizeof(vec3); }
	if (meshFlags & mesh_creation_flags_with_skin) { size += sizeof(skinning_weights); }
	return size;
}

struct cpu_mesh
{
	typedef indexed_triangle32 triangle_t;

	cpu_mesh() {}
	cpu_mesh(uint32 flags);
	cpu_mesh(const cpu_mesh& mesh) = delete;
	cpu_mesh(cpu_mesh&& mesh) = default;
	~cpu_mesh();

	uint32 flags = 0;
	uint32 othersSize = 0;
	uint32 skinOffset = 0;

	vec3* vertexPositions = 0;
	uint8* vertexOthers = 0;
	triangle_t* triangles = 0;

	uint32 numVertices = 0;
	uint32 numTriangles = 0;

	submesh_info pushQuad(vec2 radius);
	submesh_info pushCube(vec3 radius, bool flipWindingOrder = false, vec3 center = vec3(0.f, 0.f, 0.f));
	submesh_info pushSphere(uint16 slices, uint16 rows, float radius, vec3 center = vec3(0.f, 0.f, 0.f));
	submesh_info pushIcoSphere(float radius, uint32 refinement);
	submesh_info pushCapsule(uint16 slices, uint16 rows, vec3 positionA, vec3 positionB, float radius);
	submesh_info pushCapsule(uint16 slices, uint16 rows, float height, float radius, vec3 center = vec3(0.f, 0.f, 0.f), vec3 upAxis = vec3(0.f, 1.f, 0.f));
	submesh_info pushCylinder(uint16 slices, float radius, float height);
	submesh_info pushArrow(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength);
	submesh_info pushTorus(uint16 slices, uint16 segments, float torusRadius, float tubeRadius);
	submesh_info pushMace(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength);

	submesh_info pushAssimpMesh(const struct aiMesh* mesh, float scale, bounding_box* aabb = 0, animation_skeleton* skeleton = 0);

	dx_mesh createDXMesh();

private:
	typedef decltype(triangle_t::a) index_t;

	void alignNextTriangle();
	void reserve(uint32 vertexCount, uint32 triangleCount);
	void pushTriangle(index_t a, index_t b, index_t c);
};




struct vertex_uv_normal_tangent
{
	vec2 uv;
	vec3 normal;
	vec3 tangent;
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_normal[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal_tangent[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

